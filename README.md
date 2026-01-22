## About

A FSM parser for header-body structured streaming data.

## Usage

```c++
struct ProtoHeader {
  uint16_t flags;
  uint32_t body_length;
  uint16_t msg_type;
};

using ProtoParser = StreamingParser<ProtoHeader>;
ProtoParser parser(
    [](const ProtoHeader& header) {
    std::cout << "Header received: body_length=" << header.body_length << ", msg_type=" << header.msg_type
                << std::endl;
    return true;
    },
    [](const uint8_t* data, uint32_t length) {
    std::cout << "Body received: length=" << length << std::endl;
    return true;
    });

// Receiving streaming bytes from somewhere ... 
parser.HandleData(....);
```

## Compile

```bash
git submodule update --init --recursive
cmake . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## Improvement plan

- Strengthen RingBuffer invariants: validate size at runtime (non-zero, power-of-two) even in
  release builds; return a clear error or throw on invalid sizes instead of relying on asserts.
- Simplify locking: avoid recursive mutexes by keeping a single lock scope and accessing
  `buffered_bytes_` directly inside it; document thread-safety expectations and lock ordering.
- Harden index arithmetic: use `size_t` or `uint64_t` for read/write counters and mask only when
  indexing, preventing long-running overflow.
- StreamingParser error handling: respect header/body handler return values, avoid advancing state
  on failed callbacks, and surface parse errors to callers.
- Add protocol guards: enforce a maximum `body_length` (configurable) and handle zero-length bodies
  explicitly to prevent large allocations or stalled parsing.
- Byte order flexibility: introduce a trait or policy hook so callers can opt into full header
  endianness conversion without modifying the parser.
- Tests: add characterization tests for handler failure paths, zero-length bodies, and wrap-around
  reads via callback; keep tests deterministic (fixed seeds) and fast.
- Build/CI: prefer FetchContent with a URL fallback for googletest or ensure the submodule is
  initialized in CI to avoid configuration failures.
