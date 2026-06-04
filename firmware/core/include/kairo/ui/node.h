#pragma once
#include <cstdint>

namespace kairo::ui {

// Retained-mode UI node. A tree of these is built each render, laid out by the
// layout engine (layout.h), and painted by the renderer (renderer.h). Both the
// C builder (widgets.h) and a future JS reconciler produce this same structure.

enum class NodeType : uint8_t { View, Text, Pressable };
enum class FlexDir  : uint8_t { Row, Col };
enum class Align    : uint8_t { Start, Center, End, Stretch };       // cross-axis
enum class Justify  : uint8_t { Start, Center, End, SpaceBetween };  // main-axis
enum class TextRole : uint8_t { Body, Title, Caption };              // → BitmapFont

// width/height == SIZE_AUTO → measured from content; otherwise fixed logical px.
constexpr uint16_t SIZE_AUTO = 0xFFFF;

struct Style {
    FlexDir  dir        = FlexDir::Col;
    uint16_t flexGrow   = 0;            // 0 = no grow; >0 = weight sharing leftover main-axis space
    uint16_t width      = SIZE_AUTO;
    uint16_t height     = SIZE_AUTO;
    uint8_t  padding    = 0;            // uniform on 4 sides
    uint8_t  gap        = 0;            // spacing between children along main axis
    Align    align      = Align::Start;
    Justify  justify    = Justify::Start;
    bool     border     = false;        // drawRect outline
    bool     background = false;        // fillRect fill
};

struct UiNode {
    NodeType type = NodeType::View;
    Style    style;

    // Text leaf:
    const char* text = nullptr;        // NOT owned — caller guarantees lifetime through render
    TextRole    role = TextRole::Body;

    // Pressable:
    void  (*onPress)(void* userdata) = nullptr;
    void*   userdata  = nullptr;
    bool    focusable = false;

    // Tree links:
    UiNode* firstChild  = nullptr;
    UiNode* nextSibling = nullptr;

    // Layout results (filled by layout()) — absolute logical px relative to root origin:
    int16_t  x = 0, y = 0;
    uint16_t w = 0, h = 0;
};

} // namespace kairo::ui
