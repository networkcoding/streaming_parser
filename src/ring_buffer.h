/**
 * @file ring_buffer.h
 * @author Lei Peng (plhitsz@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-01-19
 *
 */
#ifndef SRC_RING_BUFFER_H_
#define SRC_RING_BUFFER_H_

#include <cstdint>
#include <functional>
#include <mutex>
#include <system_error>
#include <vector>

class RingBuffer final {
 public:
  using ReceiveCallback = std::function<bool(const uint8_t* data, uint32_t length)>;
  static const std::error_code ErrBufferOverflow;
  static const std::error_code ErrInvalidParameter;

  RingBuffer();
  explicit RingBuffer(uint32_t size);
  virtual ~RingBuffer();

  /// @brief Writes `length` bytes from `data` into the ring buffer.
  std::error_code write(const uint8_t* data, uint32_t length);

  /// @brief Read up to `length` bytes from the ring buffer into `data`.
  uint32_t read(uint8_t* data, uint32_t length);

  /// @brief Read up to `length` bytes from the ring buffer by calling the `recv_cb` callback.
  uint32_t read(uint32_t length, ReceiveCallback&& recv_cb);

  /// @brief Reset the read and write index.
  void clear();

  /// @brief Move the read index forward by `length` bytes.
  void drain(uint32_t length);

  /// @brief The capacity of the ring buffer.
  uint32_t capacity() const;

  /// @brief Buffered bytes currently stored in the ring buffer.
  uint32_t buffered_bytes() const;

  bool empty() const;
  bool full() const;
  std::string getHexString();

 private:
  mutable std::recursive_mutex mutex_;
  const uint32_t index_mask = 0;
  std::vector<uint8_t> buffer_;
  uint32_t read_index_ = 0;
  uint32_t write_index_ = 0;    // always point to the next write position
  int32_t buffered_bytes_ = 0;  // number of bytes currently buffered
};

#endif  // SRC_RING_BUFFER_H_
