#pragma once
#include <cstdint>

namespace kairo {

enum class ComponentType : uint8_t {
    Display,
    Button,
    Led,
    Sensor,
    Speaker,
    Mic,
    Camera,
    Port,
    Other,
};

struct ComponentDef {
    uint8_t       id;
    const char*   label;
    ComponentType type;
    float         x, y;
    float         w, h;
};

struct BoardProfile {
    const char*          board_id;
    const char*          board_name;
    float                board_w, board_h;
    const ComponentDef*  components;
    uint8_t              component_count;
};

} // namespace kairo
