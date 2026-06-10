#include "kairo/system/board_profile.h"
#include <cstdio>

namespace kairo {

namespace {

const char* typeName(ComponentType t) {
    switch (t) {
        case ComponentType::Display: return "display";
        case ComponentType::Button:  return "button";
        case ComponentType::Led:     return "led";
        case ComponentType::Sensor:  return "sensor";
        case ComponentType::Speaker: return "speaker";
        case ComponentType::Mic:     return "mic";
        case ComponentType::Camera:  return "camera";
        case ComponentType::Port:    return "port";
        default:                     return "other";
    }
}

void appendNum(std::string& s, float v) {
    char b[16];
    std::snprintf(b, sizeof b, "%.4g", (double)v);
    s += b;
}

} // namespace

std::string serializeBoardProfile(const BoardProfile& p) {
    std::string s;
    s.reserve(128 + (size_t)p.component_count * 96);
    s += "{\"id\":\"";
    s += p.board_id ? p.board_id : "";
    s += "\",\"name\":\"";
    s += p.board_name ? p.board_name : "";
    s += "\",\"w\":";
    appendNum(s, p.board_w);
    s += ",\"h\":";
    appendNum(s, p.board_h);
    s += ",\"components\":[";
    for (uint8_t i = 0; i < p.component_count; i++) {
        const ComponentDef& c = p.components[i];
        if (i) s += ',';
        s += "{\"id\":";
        s += std::to_string(c.id);
        s += ",\"label\":\"";
        s += c.label ? c.label : "";
        s += "\",\"type\":\"";
        s += typeName(c.type);
        s += '"';
        if (c.key != Key::None) {
            s += ",\"key\":";
            s += std::to_string((unsigned)c.key);
        }
        s += ",\"x\":";
        appendNum(s, c.x);
        s += ",\"y\":";
        appendNum(s, c.y);
        s += ",\"w\":";
        appendNum(s, c.w);
        s += ",\"h\":";
        appendNum(s, c.h);
        s += '}';
    }
    s += "]}";
    return s;
}

} // namespace kairo
