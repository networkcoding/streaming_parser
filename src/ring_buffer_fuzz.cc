#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <vector>

#include "ring_buffer.h"

namespace {

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

  uint32_t next_u32() {
    uint32_t value = 0;
    for (unsigned int i = 0; i < sizeof(value); ++i) {
      value = (value << 8) | next();
    }
    return value;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t position_ = 0;
};

void CheckState(const RingBuffer& buffer, const std::deque<uint8_t>& model) {
  Check(buffer.buffered_bytes() == model.size());
  Check(buffer.empty() == model.empty());
  Check(buffer.full() == (model.size() == buffer.capacity()));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  Input input(data, size);
  const uint32_t capacity = 1U << (1U + input.next() % 8U);
  RingBuffer buffer(capacity);
  std::deque<uint8_t> model;
  std::vector<uint8_t> write_data(4096);

  while (!input.empty()) {
    const uint8_t operation = input.next() % 7;
    const uint32_t requested = input.next_u32();

    switch (operation) {
      case 0: {
        const uint32_t length = requested % (capacity + 1U);
        for (uint32_t i = 0; i < length; ++i) {
          write_data[i] = static_cast<uint8_t>(input.next() + i);
        }
        const auto error = buffer.write(write_data.data(), length);
        if (length == 0) {
          Check(error == RingBuffer::ErrInvalidParameter);
        } else if (model.size() + length > capacity) {
          Check(error == RingBuffer::ErrBufferOverflow);
        } else {
          Check(!error);
          for (uint32_t i = 0; i < length; ++i) {
            model.push_back(write_data[i]);
          }
        }
        break;
      }
      case 1: {
        const uint32_t length = requested % (capacity * 2U + 1U);
        std::vector<uint8_t> output(std::max<uint32_t>(length, 1));
        const uint32_t expected = std::min<uint32_t>(length, model.size());
        const uint32_t read = buffer.read(output.data(), length);
        Check(read == expected);
        for (uint32_t i = 0; i < expected; ++i) {
          Check(output[i] == model.front());
          model.pop_front();
        }
        break;
      }
      case 2: {
        const uint32_t length = requested % (capacity * 2U + 1U);
        const bool accept = (input.next() & 1U) != 0;
        const uint32_t expected = std::min<uint32_t>(length, model.size());
        uint32_t callback_calls = 0;
        const uint32_t read = buffer.read(length, [&](const uint8_t* bytes, uint32_t count) {
          ++callback_calls;
          Check(count == expected);
          for (uint32_t i = 0; i < count; ++i) {
            Check(bytes[i] == model[i]);
          }
          return accept;
        });
        const bool callback_expected = length != 0 && !model.empty();
        Check(callback_calls == (callback_expected ? 1U : 0U));
        Check(read == (accept ? expected : 0U));
        if (accept) {
          for (uint32_t i = 0; i < expected; ++i) {
            model.pop_front();
          }
        }
        break;
      }
      case 3: {
        const uint32_t length = requested % (capacity * 2U + 1U);
        const uint32_t drained = std::min<uint32_t>(length, model.size());
        buffer.drain(length);
        for (uint32_t i = 0; i < drained; ++i) {
          model.pop_front();
        }
        break;
      }
      case 4:
        buffer.clear();
        model.clear();
        break;
      case 5: {
        // Keep the length near UINT32_MAX to exercise overflow-safe capacity checks.
        const uint32_t length =
            std::numeric_limits<uint32_t>::max() - requested % (model.size() + 1U);
        Check(buffer.write(write_data.data(), length) == RingBuffer::ErrBufferOverflow);
        break;
      }
      case 6: {
        Check(buffer.write(nullptr, requested) == RingBuffer::ErrInvalidParameter);
        Check(buffer.read(nullptr, requested) == 0);
        break;
      }
    }

    CheckState(buffer, model);
  }

  return 0;
}
