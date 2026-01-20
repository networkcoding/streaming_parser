#include "streaming_parser.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <vector>

struct ProtoHeader {
  uint16_t test_flag0;
  uint16_t test_flag1;
  uint32_t body_length;
  uint16_t msg_type;
};

TEST(StreamingParser, parser_init) {
  using ProtoParser = StreamingParser<ProtoHeader>;
  ProtoParser parser(
      [](const ProtoHeader& header) {
        std::cout << "Header received: body_length=" << header.body_length
                  << ", msg_type=" << header.msg_type << std::endl;
        return true;
      },
      [](const uint8_t* data, uint32_t length) {
        std::cout << "Body received: length=" << length << std::endl;
        return true;
      });
  // prepare a header
  ProtoHeader test_header;
  test_header.test_flag0 = 0xAA55;
  test_header.test_flag1 = 0xBB55;
  test_header.msg_type = 0xFFFF;
  test_header.body_length = htonl(64);

  std::vector<uint8_t> test_body(64, 0xac);

  parser.HandleData(reinterpret_cast<uint8_t*>(&test_header), sizeof(ProtoHeader));
  parser.HandleData(test_body.data(), test_body.size());
}

TEST(StreamingParser, parser_read_fuzzing) {
  using ProtoParser = StreamingParser<ProtoHeader>;
  ProtoHeader test_header;
  test_header.test_flag0 = 0xAA55;
  test_header.test_flag1 = 0xBB55;
  test_header.msg_type = 0xFFFF;
  test_header.body_length = htonl(100);
  std::vector<uint8_t> test_body(100, 0xac);

  ProtoParser parser(
      [](const ProtoHeader& header) {
        std::cout << "Header received: body_length=" << header.body_length
                  << ", msg_type=" << header.msg_type << std::endl;
        EXPECT_EQ(header.body_length, 100);
        EXPECT_EQ(header.msg_type, 0xFFFF);
        EXPECT_EQ(header.test_flag0, 0xAA55);
        EXPECT_EQ(header.test_flag1, 0xBB55);
        return true;
      },
      [](const uint8_t* data, uint32_t length) {
        std::cout << "Body received: length=" << length << std::endl;
        EXPECT_EQ(data[0], 0xac);
        EXPECT_EQ(data[length - 1], 0xac);
        return true;
      });

  for (uint32_t iter = 0; iter < 100; ++iter) {
    int header_field_send_byte = 0;  // valid range: [1, sizeof(ProtoHeader)]
    int body_field_send_byte = 0;    // valid range: [1, body_len]

    std::srand(std::time({}));  // use current time as seed for random generator
    header_field_send_byte = std::rand() % (sizeof(ProtoHeader)) + 1;
    body_field_send_byte = std::rand() % (test_body.size()) + 1;

    // part 1:
    parser.HandleData(reinterpret_cast<uint8_t*>(&test_header), header_field_send_byte);
    // part 2:
    parser.HandleData(reinterpret_cast<uint8_t*>(&test_header) + header_field_send_byte,
                      sizeof(ProtoHeader) - header_field_send_byte);

    // part 1:
    parser.HandleData(test_body.data(), body_field_send_byte);
    // part 2:
    parser.HandleData(test_body.data() + body_field_send_byte,
                      test_body.size() - body_field_send_byte);
  }
}
