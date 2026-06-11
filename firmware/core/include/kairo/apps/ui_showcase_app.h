#pragma once
#include "kairo/app/component_app.h"
#include "kairo/ui/node.h"

namespace kairo {

// UiShowcaseApp — a gallery/test-bench for the native component system
// (Plan 30/31). One app with internal pages:
//   • Menu          — pick a demo
//   • Scroll List   — long scrollable list: drag/flick (touch) or focus (buttons)
//   • Input Controls — Toggle, Stepper, Select, Slider live
// Doubles as the manual UI test surface. Lives in the Apps list (a built-in app).
class UiShowcaseApp : public ComponentApp {
public:
    UiShowcaseApp();

    const char* id()   const override { return "com.kairo.uishowcase"; }
    const char* name() const override { return "UI Showcase"; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;   // Back = page→menu / exit
    size_t      arenaCapacity() const override { return 320; }

private:
    enum class Page { Menu, ScrollList, Inputs };

    static constexpr int LIST_ROWS = 24;
    struct RowCtx { UiShowcaseApp* app; int index; };

    Page  page_ = Page::Menu;

    // Scroll-list page
    ui::ScrollState listScroll_;
    RowCtx          rows_[LIST_ROWS];
    char            listLabels_[LIST_ROWS][16];
    char            listHeader_[32] = "Tap or select a row";

    // Inputs page
    ui::ScrollState inputScroll_;
    bool            toggle_  = false;
    int             count_   = 3;
    int             selIdx_  = 1;
    int             slider_  = 40;
    char            stepBuf_[8];
    char            sliderBuf_[16];

    ui::UiNode* buildMenu(ui::NodeArena& a);
    ui::UiNode* buildScrollList(ui::NodeArena& a);
    ui::UiNode* buildInputs(ui::NodeArena& a);

    static void goScroll(void* u);
    static void goInputs(void* u);
    static void onPick(void* u);
    static void onToggle(void* u);
    static void onCountAdj(void* u, int dir);
    static void onSelAdj(void* u, int dir);
};

} // namespace kairo
