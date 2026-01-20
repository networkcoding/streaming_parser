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
