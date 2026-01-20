#include "ring_buffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>

class RingBufferErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override { return "RingBuffer"; }
  std::string message(int ev) const override {
    switch (ev) {
      case 1:
        return "Buffer Overflow";
      case 2:
        return "Invalid Buffer Parameter";
      default:
        return "Unknown Error";
    }
  }
};

const std::error_category& ring_buffer_category() {
  static RingBufferErrorCategory instance;
  return instance;
}

const std::error_code RingBuffer::ErrBufferOverflow = std::error_code(1, ring_buffer_category());
const std::error_code RingBuffer::ErrInvalidParameter = std::error_code(2, ring_buffer_category());

#define IS_POWER_OF_TWO(x) (((x) != 0) && (((x) & ((x) - 1)) == 0))

RingBuffer::RingBuffer(uint32_t size) : index_mask(static_cast<uint32_t>(size - 1)) {
  assert(size > 0);
  // "Must be power of two"
  assert(IS_POWER_OF_TWO(size));
  buffer_.resize(size);
  read_index_ = 0;
  write_index_ = 0;
}

RingBuffer::RingBuffer() : RingBuffer(2048) {
  // default size 2048 bytes
}

RingBuffer::~RingBuffer() { clear(); }

std::error_code RingBuffer::write(const uint8_t* data, uint32_t length) {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  if (data == nullptr || length == 0) {
    return ErrInvalidParameter;
  }

  if (buffered_bytes() + length > capacity()) {
    return ErrBufferOverflow;
  }

  uint32_t temp_write_idx = write_index_ & index_mask;
  if (temp_write_idx + length > capacity()) {
    size_t left = capacity() - temp_write_idx;
    std::memcpy(&buffer_[temp_write_idx], data, left);
    std::memcpy(&buffer_[0], data + left, length - left);
  } else {
    std::memcpy(&buffer_[temp_write_idx], data, length);
  }
  write_index_ = (temp_write_idx + length);
  buffered_bytes_ += length;
  assert(buffered_bytes_ >= 0);
  return std::error_code();
}

uint32_t RingBuffer::read(uint8_t* data, uint32_t length) {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  if (data == nullptr || length == 0) {
    return 0;
  }
  if (buffered_bytes_ == 0) {
    return 0;
  }
  uint32_t temp_read_idx = read_index_ & index_mask;
  uint32_t read_bytes = std::min(length, buffered_bytes());
  if (temp_read_idx + read_bytes > capacity()) {
    size_t left = capacity() - temp_read_idx;
    std::memcpy(data, &buffer_[temp_read_idx], left);
    std::memcpy(data + left, &buffer_[0], read_bytes - left);
  } else {
    std::memcpy(data, &buffer_[temp_read_idx], read_bytes);
  }
  read_index_ = (temp_read_idx + read_bytes);
  buffered_bytes_ -= read_bytes;
  assert(buffered_bytes_ >= 0);
  return read_bytes;
}

uint32_t RingBuffer::read(uint32_t length, ReceiveCallback&& recv_cb) {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  if (length == 0) {
    return 0;
  }
  if (buffered_bytes_ == 0) {
    return 0;
  }
  bool read_ok = true;
  uint32_t temp_read_idx = read_index_ & index_mask;
  uint32_t read_bytes = std::min(length, buffered_bytes());
  if (temp_read_idx + read_bytes > capacity()) {
    size_t left = capacity() - temp_read_idx;
    // optimize(.) happens when read the buffer wraps around
    std::vector<uint8_t> temp_buffer(read_bytes);
    std::memcpy(temp_buffer.data(), &buffer_[temp_read_idx], left);
    std::memcpy(temp_buffer.data() + left, &buffer_[0], read_bytes - left);
    lock.unlock();
    read_ok = recv_cb(temp_buffer.data(), read_bytes);
    lock.lock();
  } else {
    lock.unlock();
    read_ok = recv_cb(&buffer_[temp_read_idx], read_bytes);
    lock.lock();
  }
  if (read_ok) {
    read_index_ = (temp_read_idx + read_bytes);
    buffered_bytes_ -= read_bytes;
    assert(buffered_bytes_ >= 0);
    return read_bytes;
  }
  return 0;
}

void RingBuffer::clear() {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  read_index_ = 0;
  write_index_ = 0;
  buffered_bytes_ = 0;
}

void RingBuffer::drain(uint32_t length) {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  uint32_t temp_read_idx = read_index_ & index_mask;
  auto read_bytes = std::min(length, buffered_bytes());
  read_index_ = (temp_read_idx + read_bytes) & index_mask;
  buffered_bytes_ -= read_bytes;
  assert(buffered_bytes_ >= 0);
}

uint32_t RingBuffer::capacity() const {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  return static_cast<uint32_t>(buffer_.size());
}

uint32_t RingBuffer::buffered_bytes() const {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  assert(buffered_bytes_ >= 0);
  return buffered_bytes_;
}

bool RingBuffer::empty() const {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  return buffered_bytes_ == 0;
}

bool RingBuffer::full() const {
  std::unique_lock<std::recursive_mutex> lock(mutex_);
  return buffered_bytes_ == static_cast<int32_t>(buffer_.size());
}

std::string RingBuffer::getHexString() {
  uint32_t temp_read_idx = read_index_;
  std::stringstream buf_hex;
  for (size_t i = 0; i < buffered_bytes_; i++) {
    buf_hex << std::hex << static_cast<int>(buffer_[(i + temp_read_idx) & index_mask]) << " ";
  }
  return buf_hex.str();
}
