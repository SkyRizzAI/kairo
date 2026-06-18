#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nema::badusb {

struct Command {
    enum Type : uint8_t { Key, String, Delay, Repeat, RepeatEnd, None };

    Type     type = None;
    uint8_t  modifier = 0;
    uint8_t  keycode = 0;
    std::string text;
    uint32_t delayMs = 0;
    uint8_t  repeatCount = 0;
};

using Script = std::vector<Command>;

Script parse(const char* source, size_t len);
Script parseFile(const uint8_t* data, size_t len);

} // namespace nema::badusb
