#include <arpa/inet.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

#include "streaming_parser.h"

namespace {

constexpr uint32_t kMaxBodyLength = 64;
constexpr uint32_t kRingBufferCapacity = 2048;

struct FuzzHeader {
  uint16_t message_type;
  uint16_t flags;
  uint32_t body_length;
};
static_assert(sizeof(FuzzHeader) == 8);

void Check(bool condition) {
  if (!condition) {
    std::abort();
  }
}

class Input {
 public:
  Input(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  bool empty() const { return position_ == size_; }

  uint8_t next() {
    if (empty()) {
      return 0;
    }
    return data_[position_++];
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t position_ = 0;
};

struct ExpectedEvent {
  enum class Type { Header, Body };

  Type type;
  FuzzHeader header{};
  std::vector<uint8_t> body;
};

class ParserModel {
 public:
  size_t buffered_bytes() const { return pending_.size(); }

  // Model the parser's two receive states. Queue expected callbacks before real
  // delivery so the callbacks can compare the implementation against this oracle.
  // 模拟 parser 的两个接收状态。真实数据送入 parser 前先排好预期 callback，
  // 以便 callback 将实际行为与此参考模型进行比较。
  void Feed(const uint8_t* data, size_t length) {
    pending_.insert(pending_.end(), data, data + length);

    while (true) {
      if (!reading_body_) {
        if (pending_.size() < sizeof(FuzzHeader)) {
          break;
        }

        FuzzHeader header{};
        std::memcpy(&header, pending_.data(), sizeof(header));
        header.body_length = ntohl(header.body_length);
        Check(header.body_length > 0 && header.body_length <= kMaxBodyLength);
        pending_.erase(pending_.begin(), pending_.begin() + sizeof(FuzzHeader));

        ExpectedEvent event{};
        event.type = ExpectedEvent::Type::Header;
        event.header = header;
        events_.push_back(std::move(event));
        body_length_ = header.body_length;
        reading_body_ = true;
      } else {
        if (pending_.size() < body_length_) {
          break;
        }

        ExpectedEvent event{};
        event.type = ExpectedEvent::Type::Body;
        event.body.assign(pending_.begin(), pending_.begin() + body_length_);
        events_.push_back(std::move(event));
        pending_.erase(pending_.begin(), pending_.begin() + body_length_);
        reading_body_ = false;
      }
    }
  }

  void CheckHeader(const FuzzHeader& actual) {
    Check(!events_.empty());
    const ExpectedEvent& expected = events_.front();
    Check(expected.type == ExpectedEvent::Type::Header);
    Check(actual.message_type == expected.header.message_type);
    Check(actual.flags == expected.header.flags);
    Check(actual.body_length == expected.header.body_length);
    events_.pop_front();
  }

  void CheckBody(const uint8_t* actual, uint32_t length) {
    Check(!events_.empty());
    const ExpectedEvent& expected = events_.front();
    Check(expected.type == ExpectedEvent::Type::Body);
    Check(length == expected.body.size());
    Check(std::equal(expected.body.begin(), expected.body.end(), actual));
    events_.pop_front();
  }

  void CheckCallbacksComplete() const { Check(events_.empty()); }

 private:
  std::vector<uint8_t> pending_;
  std::deque<ExpectedEvent> events_;
  uint32_t body_length_ = 0;
  bool reading_body_ = false;
};

}  // namespace

// libFuzzer repeatedly calls this entry point with mutated inputs. Coverage
// instrumentation keeps inputs that reach new paths; each call gets a fresh
// parser, while commands within one input exercise multiple FSM interactions.
// libFuzzer 会反复用变异后的输入调用此入口。覆盖率插桩会保留能到达新路径的输入；
// 每次调用都会创建新的 parser，而单个输入中的多条命令会驱动多次 FSM 交互。
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  Input input(data, size);
  ParserModel model;
  StreamingParser<FuzzHeader> parser(
      [&model](const FuzzHeader& header) {
        // Any missing, extra, reordered, or incorrect callback aborts this input.
        // callback 缺失、多余、顺序错误或内容错误都会使当前输入测试失败。
        model.CheckHeader(header);
        return true;
      },
      [&model](const uint8_t* body, uint32_t length) {
        model.CheckBody(body, length);
        return true;
      });

  std::vector<uint8_t> stream;
  size_t stream_offset = 0;

  auto append_frame = [&]() {
    // Keep frames valid and bounded so fuzzing focuses on fragmentation and FSM transitions.
    // 使用合法且有界的帧，让 fuzzing 重点探索分片行为和 FSM 状态转换。
    FuzzHeader header{input.next(), input.next(), 0};
    const uint32_t body_length = 1U + input.next() % kMaxBodyLength;
    header.body_length = htonl(body_length);

    const auto* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    stream.insert(stream.end(), header_bytes, header_bytes + sizeof(header));
    for (uint32_t i = 0; i < body_length; ++i) {
      stream.push_back(input.next());
    }
  };

  auto feed_chunk = [&](size_t requested) {
    const size_t available = stream.size() - stream_offset;
    if (available == 0) {
      return;
    }

    const size_t length = std::min(requested, available);
    const uint8_t* chunk = stream.data() + stream_offset;
    // The model's pending bytes mirror RingBuffer's contents before HandleData.
    // 调用 HandleData 前，模型中未处理的字节数与 RingBuffer 中的缓冲数据一致。
    const bool fits = model.buffered_bytes() + length <= kRingBufferCapacity;

    // HandleData rejects a whole write on overflow; keep the stream offset for retry.
    // 溢出时 HandleData 会拒绝整次写入；保留流偏移，以便之后重试。
    if (fits) {
      model.Feed(chunk, length);
    }
    Check(parser.HandleData(chunk, static_cast<uint32_t>(length)) == fits);
    model.CheckCallbacksComplete();

    if (fits) {
      stream_offset += length;
      if (stream_offset == stream.size()) {
        stream.clear();
        stream_offset = 0;
      }
    }
  };

  while (!input.empty()) {
    switch (input.next() % 5U) {
      case 0:  // Generate another frame; several frames may be queued together.
               // 生成新帧；多帧可以连续排队。
        append_frame();
        break;
      case 1:  // Feed a small fragment, often splitting a header or body.
               // 送入小片段，常会将 header 或 body 拆开。
        feed_chunk(1U + input.next() % 128U);
        break;
      case 2:  // Attempt a bulk feed, which may be rejected atomically on overflow.
               // 尝试批量送入；发生溢出时整次写入会被拒绝。
        feed_chunk(kRingBufferCapacity + input.next() % kRingBufferCapacity);
        break;
      case 3:  // Drain queued stream bytes in small, varying fragments.
               // 以大小不一的小片段送完排队中的字节流。
        while (stream_offset < stream.size()) {
          feed_chunk(1U + input.next() % 128U);
        }
        break;
      case 4: {  // Exercise an empty call without changing parser state.
                 // 测试空数据调用，确认 parser 状态不变。
        const uint8_t unused = 0;
        const uint8_t* ptr = (input.next() & 1U) ? nullptr : &unused;
        Check(parser.HandleData(ptr, 0));
        model.CheckCallbacksComplete();
        break;
      }
    }
  }

  return 0;
}
