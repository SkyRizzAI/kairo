#pragma once
#include <cstdint>
#include <string>
#include "nema/ui/key.h"

namespace nema {

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
    // For Button components: the input Key a remote host (Forge) posts when this
    // button is pressed on the virtual board. None for non-button components.
    Key           key = Key::None;
};

struct BoardProfile {
    const char*          board_id;
    const char*          board_name;
    float                board_w, board_h;
    const ComponentDef*  components;
    uint8_t              component_count;
};

// Serialize a profile to the KLP SYSTEM-channel JSON (Plan 33 Phase 4 / Plan 35):
//   {"id":..,"name":..,"w":..,"h":..,
//    "components":[{"id":..,"label":..,"type":"button","key":5,"x":..,...},...]}
// Labels/ids are board-declared constants (no user input), so no JSON escaping.
std::string serializeBoardProfile(const BoardProfile& p);

} // namespace nema
