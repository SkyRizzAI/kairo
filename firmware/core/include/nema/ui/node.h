#pragma once
#include <cstdint>

namespace nema::anim { class AnimationPlayer; }

namespace nema::ui {

// Retained-mode UI node. A tree of these is built each render, laid out by the
// layout engine (layout.h), and painted by the renderer (renderer.h). Both the
// C builder (widgets.h) and a future JS reconciler produce this same structure.

enum class NodeType : uint8_t { View, Text, Pressable, Scroll, Slider, Icon, AnimatedIcon };
enum class FlexDir  : uint8_t { Row, Col };
enum class Align    : uint8_t { Start, Center, End, Stretch };       // cross-axis
enum class Justify  : uint8_t { Start, Center, End, SpaceBetween };  // main-axis
enum class TextRole : uint8_t {
    Body,     // normal text
    Title,    // large/heading
    Caption,  // small hint/label
    Mono,     // Plan 70: monospace (logs, hex, terminal)
    Smart,    // Plan 52 SmartLabel: ellipsis when too-wide+unfocused, marquee when focused
};

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

// Persistent scroll state for a NodeType::Scroll node. Lives OUTSIDE the arena
// (owned by the screen/app as a member, like a React useRef) so scroll position
// survives the per-frame tree rebuild. The builder links the node to it; layout
// writes contentMain/viewportMain + clamps scrollMain; the renderer reads
// scrollMain to offset content and size the scrollbar; gestures mutate scrollMain.
struct ScrollState {
    int16_t  scrollMain   = 0;   // current scroll offset along the scroll axis (px)
    uint16_t contentMain  = 0;   // natural total content length (filled by layout)
    uint16_t viewportMain = 0;   // visible length = inner box along scroll axis
    // Touch momentum (px per tick), set on flick release, decayed by runtime tick.
    float    velocity     = 0.0f;
    int16_t  maxScroll() const {
        return contentMain > viewportMain ? (int16_t)(contentMain - viewportMain) : 0;
    }
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

    // Value controls — Left/Right "fine adjust" for a focused control (dir −1/+1).
    // Used by Slider (and any stepper-like widget that wants arrow tuning).
    void  (*onAdjust)(void* userdata, int dir) = nullptr;

    // Slider (type == Slider): value is caller-owned so it persists across the
    // per-frame rebuild; onChange fires on drag/adjust. Knob maps min..max over w.
    int*    sliderValue = nullptr;
    int16_t sliderMin   = 0;
    int16_t sliderMax   = 100;
    int16_t sliderStep  = 1;
    void  (*onChange)(void* userdata, int value) = nullptr;

    // Icon leaf (type == Icon, Plan 53): 1-bit packed XBM bitmap.
    const uint8_t* iconBitmap = nullptr;
    uint8_t        iconW      = 0;
    uint8_t        iconH      = 0;

    // Plan 70: AnimatedIcon leaf (type == AnimatedIcon). Caller-owned player;
    // the renderer draws the current frame on each paint.
    anim::AnimationPlayer* animPlayer = nullptr;

    // Scroll container (type == Scroll): persistent state, caller-owned.
    ScrollState* scroll = nullptr;

    // Tree links:
    UiNode* firstChild  = nullptr;
    UiNode* nextSibling = nullptr;

    // Layout results (filled by layout()) — absolute logical px relative to root origin:
    int16_t  x = 0, y = 0;
    uint16_t w = 0, h = 0;
};

} // namespace nema::ui
