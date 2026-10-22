---
name: cpp-merge-request-review
description: Review C and C++ merge-request diffs for actionable defects in ownership and lifetime, RAII, iterator validity, concurrency and lock ordering, undefined behavior, and logical regressions. Use for automated or interactive C/C++ code review where findings must be evidence-based and formatted for a merge-request comment.
---

# Review a C++ merge request

Review only defects introduced or exposed by the merge-request diff. Read surrounding implementations, callers, tests, and relevant history when needed to establish impact. Do not modify files.

## Already covered - do not spend review on it

| Defect | Reported by |
|---|---|
| Unused variable, unused parameter | `-Wall` / `-Wextra` |
| Signed/unsigned comparison | `-Wextra` (`-Wsign-compare`) |
| Missing return on some path | `-Wreturn-type` (in `-Wall`) |
| Member initialisation order mismatch | `-Wreorder` (in `-Wall`) |
| `delete` through a base with no virtual dtor, direct case | `-Wdelete-non-virtual-dtor` (in `-Wall`) |

**Read the build first.** If `CMakeLists.txt` sets no warning flags, no
clang-tidy target and no sanitizer preset, then rows 1-9 are all yours. That is
one comment about the build, followed by the specific instances — not a
per-line campaign.

## Checks

Prioritize:

1. Memory ownership and lifetime
   - Detect leaks, double frees, use-after-free, dangling pointers/references/views, invalid captures, ownership ambiguity, and missing RAII.
   - Check iterator, pointer, and reference invalidation after mutation, reallocation, erase, moves, callbacks, or asynchronous work.
2. Concurrency
   - Detect data races, deadlocks, inconsistent lock ordering, missing synchronization, unsafe callback re-entry, incorrect atomic ordering, and object lifetime races.
   - Trace all shared-state access and lock acquisition paths before reporting a finding.
3. Undefined behavior
   - Check bounds, invalid shifts, signed overflow, uninitialized reads, invalid casts, alignment, strict aliasing, iterator misuse, null dereferences, format mismatches, and object-lifetime violations.
4. Logical regressions
   - Compare changed behavior with existing callers, tests, invariants, protocols, error handling, and previous behavior.
   - Report changes that can break existing functionality, including edge cases, cleanup paths, state transitions, retries, timeouts, and platform-specific behavior.

Skip style-only feedback, speculative concerns without a reachable failure mode, pre-existing issues unrelated to the diff, praise, and broad refactoring suggestions.

## Other checks on C++ code that earn their place

### A guard that guards nothing

```cpp
std::lock_guard<std::mutex>(m);        // a TEMPORARY: destroyed at the semicolon
std::lock_guard<std::mutex> guard(m);  // holds until end of scope
```

**When you see it**: any RAII type constructed without a variable name —
`lock_guard`, `unique_lock`, `scoped_lock`, a timer, a transaction guard.

**Ask**: does this have a name? The unnamed form locks and unlocks on the same
line, so the critical section below runs unprotected. It compiles, it looks
right, and the race it creates is intermittent. (Some compilers warn under
`-Wunused-value`; many do not for types with non-trivial destructors.)

### A `string_view` or reference outliving what it points into

```cpp
std::string_view name = person().full_name();   // temporary dies at the semicolon
return name;                                    // dangling
```

**When you see it**: `string_view`, `span`, a reference or a raw pointer
initialised from a function call returning by value; a `string_view` stored as a
class member.

**Ask**: who owns the buffer, and does it outlive this? `string_view` and `span`
are non-owning by design, so every one of them is a lifetime claim. A member of
that type is a claim about an object the class does not control.

### A range-for over a temporary's subobject

```cpp
for (const auto& row : getTable().rows()) { }   // getTable() dies before the loop
```

**When you see it**: `for (... : expr)` where `expr` is a member access, an index
or a call *on* a temporary rather than the temporary itself.

**Ask**: is the temporary itself bound, or something inside it? Lifetime
extension applies to the whole temporary bound directly to the reference — and
not to a subobject reached through a call. C++23 fixed this for the direct case
only; the shape above still dangles.

### An iterator used after the container was modified

```cpp
for (auto it = v.begin(); it != v.end(); ++it) {
    if (pred(*it)) v.push_back(*it);   // may reallocate; `it` and `end()` dangle
}
```

**When you see it**: `push_back`, `insert`, `erase` or `resize` inside a loop
over the same container.

**Ask**: can this reallocate or shift? For `vector` any growth invalidates
everything; `erase` invalidates from the erase point on. The `erase` case has a
correct idiom (`it = v.erase(it)`) that is one character from the wrong one.

### An owning raw pointer member with compiler-generated copy

```cpp
class Buffer {
    char* data_;             // owns
public:
    ~Buffer() { delete[] data_; }
    // no copy ctor, no copy assignment declared
};
```

**When you see it**: a destructor that frees, with no user-declared copy/move
operations — the Rule of Three/Five broken in the direction that compiles.

**Ask**: what happens when this is copied? The implicit copy duplicates the
pointer, and both objects free it: double free, at a moment unrelated to the
copy. Declaring the destructor does not suppress the copy operations. `= delete`
or a smart pointer member is the fix.

### A derived object sliced on copy

```cpp
void render(Shape s);        // by value
render(circle);              // the Circle part is cut away
std::vector<Shape> shapes;   // same, on every push_back
```

**When you see it**: a polymorphic type passed, returned or stored **by value**;
a container of a base class.

**Ask**: is this type ever derived from? Slicing is silent — the program runs
and calls the base implementation. No warning covers it, because by-value
passing is legitimate for non-polymorphic types.

### A `shared_ptr` that does not share the same control block

```cpp
class Session {
    void register_self() { registry.add(std::shared_ptr<Session>(this)); }
};
```

**When you see it**: a `shared_ptr` constructed from a raw `this`, or twice from
the same raw pointer.

**Ask**: how many control blocks now exist for this object? Two owners each
believing they are the last, so it is freed twice. `enable_shared_from_this` is
the mechanism that avoids it. Also check the cycle case: two `shared_ptr`s
pointing at each other never reach zero, and nothing reports the leak.

### A `const` method handing out a mutable reference

```cpp
class Config {
    std::map<std::string, std::string> values_;
public:
    std::map<std::string, std::string>& all() const { return values_; }  // needs mutable/const_cast to compile
    const std::vector<Item>& items() const { return items_; }            // fine
};
```

**When you see it**: a getter returning a non-const reference or pointer to a
member, especially from a `const` method or on a class documented as immutable.

**Ask**: does the class still control its invariants? `const` on the method
protects the pointer, not what it points at, and a returned handle outlives the
call. Note this is also the shape that makes thread-safety reasoning impossible.

### An object used after being moved from

```cpp
process(std::move(cfg));
if (cfg.enabled) { }        // valid but unspecified state
```

**When you see it**: a variable read after appearing in a `std::move`, or moved
inside a loop body.

**Ask**: is this read guaranteed? A moved-from standard type is valid but
*unspecified* — `.empty()` is allowed to be false. It is not UB, which is why it
survives sanitizers, and it is why the bug reproduces on one standard library
and not another. `bugprone-use-after-move` finds the direct shape when
clang-tidy runs.

### Static initialisation order across translation units

```cpp
// a.cpp
extern Logger logger;
Registry registry{logger};   // logger may not be constructed yet
```

**When you see it**: a namespace-scope object whose constructor uses another
namespace-scope object from a different `.cpp`.

**Ask**: is the order defined? Across translation units it is not, and the
failure depends on link order — so it appears when an unrelated file is added.
A function-local static (constructed on first use) is the standard fix.

## Validate findings

For each candidate issue:

- Identify a concrete input, state, or interleaving that triggers it.
- Verify the cited line is changed by the merge request and is the best location for the comment.
- Inspect guards, ownership transfer, synchronization, and callers that could disprove it.
- Assign `P0`, `P1`, `P2`, or `P3`; omit issues below `P3`.
- Keep line ranges minimal and recommendations concise.

## Output

Return Markdown with exactly these top-level sections:

```markdown
## Summary
<Brief description of the change and overall risk.>

## Found issues

### [P1] <Actionable title>
- **Location:** `path/to/file.cpp:123`
- **Category:** Memory lifetime | Concurrency | Undefined behavior | Logical regression
- **Impact:** <What breaks and under which conditions.>
- **Evidence:** <Concrete control flow, state, input, or interleaving.>
- **Recommendation:** <Smallest practical correction.>
```

Order issues by severity. If there are no qualifying findings, write `No issues found.` under `## Found issues`. Do not add other top-level sections.
