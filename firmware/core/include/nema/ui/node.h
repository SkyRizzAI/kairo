#pragma once
#include <cstdint>

namespace nema::anim { class AnimationPlayer; }

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

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
    Subhead,  // Plan 79: bold section header (smaller than Title) — ListView sections
};

// width/height == SIZE_AUTO → measured from content; otherwise fixed logical px.
constexpr uint16_t SIZE_AUTO = 0xFFFF;

enum class Position : uint8_t { Relative, Absolute };

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
    uint8_t  cornerRadius = 1;          // rounded-rect radius for border/background
    // Flipper-style focus highlight: when this focusable node is the focused one,
    // the renderer paints a rounded (r=1) inverted box over its bbox instead of a
    // square full-bleed invert. Used by ListView rows (Layer 3).
    bool     selectBox  = false;
    // flex-basis: 0 — when set (with flexGrow>0), the node's own content size is
    // ignored when distributing main-axis space, so grow children split the line
    // by pure ratio regardless of their content width. Lets a list row keep a
    // FIXED label/value split independent of label length (content clips/marquees).
    bool     flexZero   = false;

    // F2.1 — Per-side margins (independent of padding, applied in layout after flex)
    int8_t  mt = 0, mr = 0, mb = 0, ml = 0;  // margin top/right/bottom/left

    // F2.2 — Size constraints: clamp after flex distribution
    uint16_t minW = 0;
    uint16_t maxW = SIZE_AUTO;   // SIZE_AUTO = unconstrained
    uint16_t minH = 0;
    uint16_t maxH = SIZE_AUTO;

    // F2.4 — Absolute positioning: remove from flex flow, pin to parent origin
    Position position = Position::Relative;
    int16_t  absX = 0;   // relative to parent top-left
    int16_t  absY = 0;

    // Overflow:hidden — clip this node's own drawing (and children) to its bbox.
    // Used by the footer-legend collapse animation to truncate a shrinking label
    // cleanly instead of letting the text overflow the narrowing pill.
    bool     clip = false;

    // Scales an auto-width Text node's MEASURED content width (1.0 = natural).
    // Lets the layout engine shrink a label without the caller measuring text —
    // the footer-legend collapse animates this 1→0. Ignored when width is fixed.
    float    widthScale = 1.0f;
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
    const char* key  = nullptr;   // F3.2: stable id for reconciliation (optional)
    TextRole    role = TextRole::Body;

    // Pressable:
    void  (*onPress)(void* userdata) = nullptr;
    void*   userdata  = nullptr;
    bool    focusable     = false;
    // Plan 90 F6.04: VirtualList items can't use the focus-tree pointer match
    // (VirtualList forces focusable=false). Set selfHighlight=true to get the
    // same XOR inversion that highlightBox() applies to the focused Pressable.
    bool    selfHighlight = false;

    // Value controls — Left/Right "fine adjust" for a focused control (dir −1/+1).
    // Used by Slider (and any stepper-like widget that wants arrow tuning).
    void  (*onAdjust)(void* userdata, int dir) = nullptr;

    // Slider (type == Slider): value is caller-owned so it persists across the
    // per-frame rebuild; onChange fires on drag/adjust. Knob maps min..max over w.
    int*    sliderValue = nullptr;
    int16_t sliderMin   = 0;
    int16_t sliderMax   = 100;
    int16_t sliderStep  = 1;
    bool    sliderVertical = false;   // draw the bar vertically (fill from bottom)
    void  (*onChange)(void* userdata, int value) = nullptr;

    // Icon leaf (type == Icon, Plan 53): 1-bit packed XBM bitmap.
    // Also reused by Slider as a centred overlay glyph (drawn XOR).
    const uint8_t* iconBitmap = nullptr;
    uint8_t        iconW      = 0;
    uint8_t        iconH      = 0;
    uint8_t        iconScale  = 1;   // pixel-double factor (1 = native)

    // F2.3 — Multiline text: wrap at node width, limited to maxLines (0=unlimited)
    bool    wrap     = false;
    uint8_t lineGap  = 1;    // px between lines
    uint8_t maxLines = 0;    // 0 = unlimited

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

} // namespace aether::ui
