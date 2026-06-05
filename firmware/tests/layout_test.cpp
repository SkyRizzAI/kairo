// Host unit test for the flexbox-subset layout engine (Plan 27, Phase 1).
// Pure logic — uses a fake TextMetrics (no Canvas/display). Build & run on host.
#include "kairo/ui/node.h"
#include "kairo/ui/widgets.h"
#include "kairo/ui/layout.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace kairo::ui;

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); g_fail++; } \
    else         { std::printf("  ok:   %s\n", msg); } \
} while (0)

// Fake text metrics: 6px per char wide, 8px tall (matches FONT_5X8-ish).
static uint16_t fakeW(void*, const char* t, TextRole) {
    return (uint16_t)(t ? std::strlen(t) * 6 : 0);
}
static uint16_t fakeH(void*, TextRole) { return 8; }
static TextMetrics TM{ fakeW, fakeH, nullptr };

static void test_col_grow() {
    std::printf("[col grow: 3 children, middle grows, height 100]\n");
    NodeArena a(64);
    // Col height 100; three fixed-height(10) views, middle has flexGrow=1.
    Style fixed; fixed.height = 10; fixed.width = 50;
    Style grow = fixed; grow.flexGrow = 1;
    UiNode* root = View(a, [](){ Style s; s.dir = FlexDir::Col; s.width = 50; s.height = 100; return s; }(),
                        { View(a, fixed, {}), View(a, grow, {}), View(a, fixed, {}) });
    layout(*root, 50, 100, TM);

    UiNode* c0 = root->firstChild;
    UiNode* c1 = c0->nextSibling;
    UiNode* c2 = c1->nextSibling;
    CHECK(c0->y == 0,   "child0 y == 0");
    CHECK(c0->h == 10,  "child0 h == 10");
    CHECK(c1->y == 10,  "child1 y == 10 (after child0)");
    CHECK(c1->h == 80,  "child1 h == 80 (grew to fill 100-10-10)");
    CHECK(c2->y == 90,  "child2 y == 90");
    CHECK(c2->h == 10,  "child2 h == 10");
}

static void test_row_spacebetween() {
    std::printf("[row space-between: 2 boxes in width 100]\n");
    NodeArena a(64);
    Style box; box.width = 20; box.height = 10;
    Style row; row.dir = FlexDir::Row; row.width = 100; row.height = 10;
    row.justify = Justify::SpaceBetween;
    UiNode* root = View(a, row, { View(a, box, {}), View(a, box, {}) });
    layout(*root, 100, 10, TM);

    UiNode* c0 = root->firstChild;
    UiNode* c1 = c0->nextSibling;
    CHECK(c0->x == 0,  "box0 x == 0 (start)");
    CHECK(c1->x == 80, "box1 x == 80 (end, 100-20)");
}

static void test_align_center_padding_gap() {
    std::printf("[col align=center, padding=4, gap=2]\n");
    NodeArena a(64);
    Style col; col.dir = FlexDir::Col; col.width = 100; col.height = 100;
    col.align = Align::Center; col.padding = 4; col.gap = 2;
    Style box; box.width = 20; box.height = 10;
    UiNode* root = View(a, col, { View(a, box, {}), View(a, box, {}) });
    layout(*root, 100, 100, TM);

    UiNode* c0 = root->firstChild;
    UiNode* c1 = c0->nextSibling;
    // inner width = 100-8=92; centered box(20) → x = 4 + (92-20)/2 = 4+36 = 40
    CHECK(c0->x == 40, "box0 x centered == 40");
    CHECK(c0->y == 4,  "box0 y == padding 4");
    CHECK(c1->y == 16, "box1 y == 4 + 10 + gap2 == 16");
}

static void test_text_measure() {
    std::printf("[text intrinsic size via metrics — Text is a leaf inside a View]\n");
    NodeArena a(64);
    // Auto-sized Col wrapping a Text. Root fills viewport, but the Text child
    // keeps its measured intrinsic size (align defaults to Start, not Stretch).
    Style col; col.dir = FlexDir::Col;   // AUTO width/height
    UiNode* t = Text(a, "Hello");        // 5 chars * 6 = 30 wide, 8 tall
    UiNode* root = View(a, col, { t });
    layout(*root, 200, 100, TM);
    CHECK(t->w == 30, "text w == 30 (5*6)");
    CHECK(t->h == 8,  "text h == 8");
    CHECK(t->x == 0,  "text x == 0");
    CHECK(t->y == 0,  "text y == 0");
}

static void test_scroll_overflow() {
    std::printf("[scroll: bounded viewport in flex parent, content overflows]\n");
    NodeArena a(128);
    ScrollState st;
    // Parent Col height 100: fixed header(20) + ScrollView(flex:1) holding 10
    // boxes of height 20 (content 200 > viewport 80).
    Style header; header.height = 20; header.width = 50;
    Style svStyle; svStyle.dir = FlexDir::Col;   // flexGrow defaults to 1 in builder
    UiNode* boxes[10];
    Style box; box.width = 50; box.height = 20;
    UiNode* sv = ScrollView(a, st, svStyle, {});
    UiNode* prev = nullptr;
    for (int i = 0; i < 10; i++) {
        boxes[i] = View(a, box, {});
        if (!prev) sv->firstChild = boxes[i]; else prev->nextSibling = boxes[i];
        prev = boxes[i];
    }
    Style rootS; rootS.dir = FlexDir::Col; rootS.width = 50; rootS.height = 100;
    UiNode* root = View(a, rootS, { View(a, header, {}), sv });
    layout(*root, 50, 100, TM);

    CHECK(sv->h == 80, "scrollview viewport h == 100-20 == 80 (flex bounded)");
    CHECK(st.contentMain == 200, "content natural length == 10*20 == 200");
    CHECK(st.viewportMain == 80, "viewport recorded == 80");
    CHECK(st.maxScroll() == 120, "maxScroll == 200-80 == 120");
    CHECK(boxes[0]->y == 20, "box0 y == header bottom (scrollY 0)");

    // Scroll down by 50 → children shift up by 50.
    st.scrollMain = 50;
    layout(*root, 50, 100, TM);
    CHECK(boxes[0]->y == 20 - 50, "box0 y shifted up by scroll 50 == -30");
    CHECK(st.scrollMain == 50, "scroll stays 50 (within bounds)");

    // Over-scroll clamps to maxScroll.
    st.scrollMain = 999;
    layout(*root, 50, 100, TM);
    CHECK(st.scrollMain == 120, "over-scroll clamped to maxScroll 120");
}

int main() {
    std::printf("== Layout engine tests ==\n");
    test_col_grow();
    test_row_spacebetween();
    test_align_center_padding_gap();
    test_text_measure();
    test_scroll_overflow();
    std::printf("== %s ==\n", g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
