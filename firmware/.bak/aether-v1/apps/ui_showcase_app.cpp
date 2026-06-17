#include "nema/apps/ui_showcase_app.h"
#include "nema/ui/widgets.h"
#include <cstdio>

namespace nema {

using namespace ui;

static const char* kSelOpts[] = {"Low", "Mid", "High"};

UiShowcaseApp::UiShowcaseApp() {
    for (int i = 0; i < LIST_ROWS; i++) {
        rows_[i] = {this, i};
        std::snprintf(listLabels_[i], sizeof(listLabels_[i]), "Item %d", i + 1);
    }
}

// ── navigation between pages ───────────────────────────────────────────────
void UiShowcaseApp::goScroll(void* u) { static_cast<UiShowcaseApp*>(u)->page_ = Page::ScrollList; }
void UiShowcaseApp::goInputs(void* u) { static_cast<UiShowcaseApp*>(u)->page_ = Page::Inputs; }

bool UiShowcaseApp::onKey(Key k, AppContext&) {
    if (k == Key::Cancel && page_ != Page::Menu) { page_ = Page::Menu; return true; }
    return false;   // Cancel on the menu falls through → exit app
}

// ── scroll list page ───────────────────────────────────────────────────────
void UiShowcaseApp::onPick(void* u) {
    auto* r = static_cast<RowCtx*>(u);
    std::snprintf(r->app->listHeader_, sizeof(r->app->listHeader_), "Picked: Item %d", r->index + 1);
}

// ── inputs page callbacks ──────────────────────────────────────────────────
void UiShowcaseApp::onToggle(void* u) { auto* s = static_cast<UiShowcaseApp*>(u); s->toggle_ = !s->toggle_; }
void UiShowcaseApp::onCountAdj(void* u, int dir) {
    auto* s = static_cast<UiShowcaseApp*>(u);
    s->count_ += dir; if (s->count_ < 0) s->count_ = 0; if (s->count_ > 99) s->count_ = 99;
}
void UiShowcaseApp::onSelAdj(void* u, int dir) {
    auto* s = static_cast<UiShowcaseApp*>(u);
    s->selIdx_ += dir; if (s->selIdx_ < 0) s->selIdx_ = 0; if (s->selIdx_ > 2) s->selIdx_ = 2;
}

UiNode* UiShowcaseApp::buildMenu(NodeArena& a) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 4;
    root.align = Align::Stretch;
    Style titleRow; titleRow.dir = FlexDir::Row; titleRow.justify = Justify::Center;
    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 3;
    menu.flexGrow = 1; menu.justify = Justify::Center;
    return View(a, root, {
        Row(a, titleRow, { Text(a, "UI Showcase", TextRole::Title) }),
        Col(a, menu, {
            ListRow(a, "Scroll List",    goScroll, this),
            ListRow(a, "Input Controls", goInputs, this),
        }),
        Footer(a, "Back exits"),
    });
}

UiNode* UiShowcaseApp::buildScrollList(NodeArena& a) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;
    UiNode* list = ScrollView(a, listScroll_, sv, {});
    UiNode* prev = nullptr;
    for (int i = 0; i < LIST_ROWS; i++) {
        UiNode* row = ListRow(a, listLabels_[i], onPick, &rows_[i]);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }
    return View(a, root, {
        Text(a, listHeader_, TextRole::Title),
        View(a, line, {}),
        list,
        Footer(a, "Drag/flick or focus • Back"),
    });
}

UiNode* UiShowcaseApp::buildInputs(NodeArena& a) {
    std::snprintf(stepBuf_,   sizeof(stepBuf_),   "%d", count_);
    std::snprintf(sliderBuf_, sizeof(sliderBuf_), "Brightness %d%%", slider_);

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 3;

    return View(a, root, {
        Text(a, "Input Controls", TextRole::Title),
        View(a, line, {}),
        ScrollView(a, inputScroll_, sv, {
            Toggle(a, "Wi-Fi", toggle_, onToggle, this),
            Stepper(a, "Count", stepBuf_, onCountAdj, this),
            Select(a, "Quality", kSelOpts[selIdx_], onSelAdj, this),
            Text(a, sliderBuf_, TextRole::Caption),
            Slider(a, &slider_, 0, 100, 5, nullptr, this),
            Footer(a, "Left/Right adjust slider • Back"),
        }),
    });
}

UiNode* UiShowcaseApp::build(NodeArena& a, AppContext&) {
    switch (page_) {
        case Page::ScrollList: return buildScrollList(a);
        case Page::Inputs:     return buildInputs(a);
        case Page::Menu:
        default:               return buildMenu(a);
    }
}

} // namespace nema
