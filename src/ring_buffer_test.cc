#include "ring_buffer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

TEST(RingBuffer, buffer_init) {
// check assert error for non-power-of-two size only in debug mode
#ifndef NDEBUG
  EXPECT_DEATH({ auto buffer = std::make_shared<RingBuffer>(3); }, ".*");
  EXPECT_DEATH({ auto buffer = std::make_shared<RingBuffer>(0); }, ".*");
#endif
  for (auto buffer_length : {2, 16, 1024, 4096}) {
    auto buffer = std::make_shared<RingBuffer>(buffer_length);
    EXPECT_EQ(buffer->buffered_bytes(), 0);
    EXPECT_EQ(buffer->capacity(), buffer_length);

    buffer->drain(1);
    EXPECT_EQ(buffer->buffered_bytes(), 0);

    buffer->clear();
    EXPECT_EQ(buffer->buffered_bytes(), 0);
    EXPECT_EQ(buffer->capacity(), buffer_length);

    buffer->drain(1);
    EXPECT_EQ(buffer->buffered_bytes(), 0);
    EXPECT_EQ(buffer->capacity(), buffer_length);
  }
}

TEST(RingBuffer, buffer_write_drain_test) {
  for (auto buffer_length : {2, 8, 16, 1024, 4096}) {
    auto buffer = std::make_shared<RingBuffer>(buffer_length);
    assert(buffer->capacity() > 1);

    // write data `capacity - 1`
    std::vector<uint8_t> write_data(buffer_length, 0x5A);
    auto err = buffer->write(write_data.data(), buffer->capacity() - 1);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() - 1);

    // write one more byte
    err = buffer->write(write_data.data(), 1);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());

    // write one byte to overflow the buffer
    err = buffer->write(write_data.data(), 1);
    EXPECT_EQ(err, RingBuffer::ErrBufferOverflow);

    // drain one byte
    buffer->drain(1);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() - 1);

    // drain all bytes
    buffer->drain(buffer->capacity() - 1);
    EXPECT_EQ(buffer->buffered_bytes(), 0);
    EXPECT_EQ(buffer->capacity(), buffer_length);
  }
}

TEST(RingBuffer, buffer_write_read_test) {
  for (auto buffer_length : {32, 1024, 4096}) {
    auto buffer = std::make_shared<RingBuffer>(buffer_length);
    assert(buffer->capacity() > 1);

    // write data `capacity`
    std::vector<uint8_t> write_data(buffer_length, 0x5A);
    auto err = buffer->write(write_data.data(), buffer->capacity());
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());

    // read all data
    std::vector<uint8_t> read_data(buffer_length, 0x00);
    uint32_t read_bytes = buffer->read(read_data.data(), buffer->capacity());
    EXPECT_EQ(read_bytes, buffer->capacity());
    EXPECT_EQ(buffer->buffered_bytes(), 0);
    EXPECT_EQ(read_data, write_data);

    // write 10 bytes
    err = buffer->write(write_data.data(), 10);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), 10);

    // write 20 bytes
    err = buffer->write(write_data.data(), 20);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), 30);

    // read 15 bytes
    read_bytes = buffer->read(read_data.data(), 15);
    EXPECT_EQ(read_bytes, 15);
    EXPECT_EQ(buffer->buffered_bytes(), 15);

    // overflow write
    err = buffer->write(write_data.data(), buffer->capacity());
    EXPECT_EQ(err, RingBuffer::ErrBufferOverflow);
    EXPECT_EQ(buffer->buffered_bytes(), 15);

    // invalid write
    err = buffer->write(nullptr, 10);
    EXPECT_EQ(err, RingBuffer::ErrInvalidParameter);
    EXPECT_EQ(buffer->buffered_bytes(), 15);

    // invalid write
    err = buffer->write(write_data.data(), 0);
    EXPECT_EQ(err, RingBuffer::ErrInvalidParameter);
    EXPECT_EQ(buffer->buffered_bytes(), 15);

    // continuous read: 5 + 2 + 8
    read_bytes = buffer->read(read_data.data(), 5);
    EXPECT_EQ(read_bytes, 5);
    EXPECT_EQ(buffer->buffered_bytes(), 10);

    read_bytes = buffer->read(read_data.data(), 2);
    EXPECT_EQ(read_bytes, 2);
    EXPECT_EQ(buffer->buffered_bytes(), 8);

    read_bytes = buffer->read(read_data.data(), 8);
    EXPECT_EQ(read_bytes, 8);
    EXPECT_EQ(buffer->buffered_bytes(), 0);

    // write half capacity
    err = buffer->write(write_data.data(), buffer->capacity() / 2);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() / 2);
  }
}

TEST(RingBuffer, buffer_write_read_edge_case) {
  for (auto buffer_length : {2, 16, 1024, 4096}) {
    auto buffer = std::make_shared<RingBuffer>(buffer_length);
    // write `buffer_length`
    std::vector<uint8_t> write_data(buffer_length, 0x5A);
    auto err = buffer->write(write_data.data(), buffer->capacity());
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());
    // drain all
    buffer->drain(buffer->capacity());
    EXPECT_EQ(buffer->buffered_bytes(), 0);

    // write `buffer_length - 1` again
    err = buffer->write(write_data.data(), buffer->capacity() - 1);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() - 1);

    // drain all
    buffer->drain(buffer->capacity() - 1);
    EXPECT_EQ(buffer->buffered_bytes(), 0);
  }
}

TEST(RingBuffer, buffer_overflow_test) {
  for (auto buffer_length : {2, 8, 16, 1024, 4096}) {
    auto buffer = std::make_shared<RingBuffer>(buffer_length);
    assert(buffer->capacity() > 1);
    std::vector<uint8_t> write_data(buffer_length, 0x5A);
    // write `capacity + 1`
    auto err = buffer->write(write_data.data(), buffer->capacity() + 1);
    EXPECT_EQ(err, RingBuffer::ErrBufferOverflow);
    EXPECT_EQ(buffer->buffered_bytes(), 0);

    // write `capacity`
    err = buffer->write(write_data.data(), buffer->capacity());
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());

    // write one more byte
    err = buffer->write(write_data.data(), 1);
    EXPECT_EQ(err, RingBuffer::ErrBufferOverflow);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());

    // drain one bytes
    buffer->drain(1);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() - 1);

    // writable again
    err = buffer->write(write_data.data(), 1);
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());

    // drain all bytes
    buffer->drain(buffer->capacity());
    EXPECT_EQ(buffer->buffered_bytes(), 0);

    // write full
    err = buffer->write(write_data.data(), buffer->capacity());
    EXPECT_FALSE(err);
    EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());
  }
}

TEST(RingBuffer, buffer_read_write_content_validation) {
  auto buffer_length = 256;
  auto buffer = std::make_shared<RingBuffer>(buffer_length);
  std::vector<uint8_t> write_data(buffer_length);
  for (uint32_t i = 0; i < buffer_length; ++i) {
    write_data[i] = static_cast<uint8_t>(i);
  }

  // write full buffer
  auto err = buffer->write(write_data.data(), buffer->capacity());
  EXPECT_FALSE(err);
  EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity());
  // read full buffer
  std::vector<uint8_t> read_data(buffer_length, 0x00);
  uint32_t read_bytes = buffer->read(read_data.data(), buffer->capacity());
  EXPECT_EQ(read_bytes, buffer->capacity());
  EXPECT_EQ(buffer->buffered_bytes(), 0);
  EXPECT_EQ(read_data, write_data);

  // write 1/4 buffer
  err = buffer->write(write_data.data(), buffer->capacity() / 4);
  EXPECT_FALSE(err);
  EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() / 4);

  // write another 1/4 buffer
  err = buffer->write(write_data.data() + buffer->capacity() / 4, buffer->capacity() / 4);
  EXPECT_FALSE(err);

  // read 2/5 buffer
  auto to_read = buffer->capacity() * 2 / 5;
  read_bytes = buffer->read(read_data.data(), to_read);
  EXPECT_EQ(read_bytes, to_read);
  EXPECT_EQ(buffer->buffered_bytes(), buffer->capacity() / 2 - to_read);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + to_read),
            std::vector<uint8_t>(write_data.begin(), write_data.begin() + to_read));

  // read remaining buffer
  read_bytes = buffer->read(read_data.data(), buffer->capacity());
  EXPECT_EQ(read_bytes, buffer->capacity() / 2 - to_read);
  EXPECT_EQ(buffer->buffered_bytes(), 0);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + read_bytes),
            std::vector<uint8_t>(write_data.begin() + to_read,
                                 write_data.begin() + to_read + read_bytes));
}