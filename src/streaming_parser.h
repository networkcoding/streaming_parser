/**
 * @file streaming_parser.h
 * @author Lei Peng (plhitsz@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-01-20
 *
 */
#ifndef SRC_STREAMING_PARSER_H_
#define SRC_STREAMING_PARSER_H_

#include <arpa/inet.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

#include "ring_buffer.h"

template <typename T, typename = void>
struct has_body_length : std::false_type {};

template <typename T>
struct has_body_length<T, std::void_t<decltype(std::declval<T>().body_length)>> : std::true_type {};

/// @brief A FSM parser for header-body structured streaming data.
/// @tparam ProtoHeader The protocol header struct type. It must contain a field named `body_length`
/// of type uint16_t or uint32_t.
template <typename ProtoHeader>
class StreamingParser {
 public:
  constexpr static uint32_t protocol_header_length = sizeof(ProtoHeader);
  static_assert(std::is_same_v<decltype(std::declval<ProtoHeader>().body_length), uint16_t> ||
                    std::is_same_v<decltype(std::declval<ProtoHeader>().body_length), uint32_t>,
                "ProtoHeader body_length field must be uint16_t or uint32_t");
  using HeaderHandler = std::function<bool(const ProtoHeader& header)>;
  using BodyHandler = std::function<bool(const uint8_t* data, uint32_t length)>;
  StreamingParser(HeaderHandler&& header_handler, BodyHandler&& body_handler)
      : header_handler_(std::move(header_handler)), body_handler_(std::move(body_handler)) {}
  virtual ~StreamingParser() = default;

  /// @brief The interface to feed data into the parser.
  bool HandleData(const uint8_t* data, uint32_t length);

 private:
  void DoBytesOrderConversion(ProtoHeader& header);
  bool PerformStreamingParse();

  enum class RecvState : uint8_t {
    READ_HEADER,
    READ_BODY,
  };
  RecvState recv_state_ = RecvState::READ_HEADER;
  ProtoHeader current_header_;
  HeaderHandler header_handler_;
  BodyHandler body_handler_;
  RingBuffer recv_buffer_;
};

template <typename ProtoHeader>
void StreamingParser<ProtoHeader>::DoBytesOrderConversion(ProtoHeader& header) {
  if constexpr (sizeof(header.body_length) == sizeof(uint16_t)) {
    header.body_length = ntohs(header.body_length);
  } else if constexpr (sizeof(header.body_length) == sizeof(uint32_t)) {
    header.body_length = ntohl(header.body_length);
  }
}

template <typename ProtoHeader>
bool StreamingParser<ProtoHeader>::HandleData(const uint8_t* data, uint32_t length) {
  auto err = recv_buffer_.write(data, length);
  if (err == RingBuffer::ErrBufferOverflow) {
    return false;
  }
  while ((recv_state_ == RecvState::READ_HEADER &&
          recv_buffer_.buffered_bytes() >= protocol_header_length) ||
         (recv_state_ == RecvState::READ_BODY &&
          recv_buffer_.buffered_bytes() >= current_header_.body_length)) {
    if (PerformStreamingParse()) {
      // waiting for more bytes to proceed
      break;
    }
  }
  return true;
}

template <typename ProtoHeader>
bool StreamingParser<ProtoHeader>::PerformStreamingParse() {
  switch (recv_state_) {
    case RecvState::READ_HEADER:
      if (recv_buffer_.buffered_bytes() >= protocol_header_length) {
        recv_buffer_.read(reinterpret_cast<uint8_t*>(&current_header_), protocol_header_length);
        DoBytesOrderConversion(current_header_);
        header_handler_(current_header_);
        recv_state_ = RecvState::READ_BODY;
      } else {
        // length field not ready.
        return true;
      }
      break;
    case RecvState::READ_BODY:
      assert(current_header_.body_length > 0);
      if (recv_buffer_.buffered_bytes() >= current_header_.body_length) {
        recv_buffer_.read(
            current_header_.body_length,
            [this](const uint8_t* data, uint32_t length) { return body_handler_(data, length); });
        recv_state_ = RecvState::READ_HEADER;
        current_header_.body_length = 0;
      } else {
        // body field not ready.
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

#endif  // SRC_STREAMING_PARSER_H_
