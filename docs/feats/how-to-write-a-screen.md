# How to Write a Palanu Screen

This tutorial walks through writing a new UI screen using the Aether component system.
`HelloApp` (in `firmware/core/src/apps/hello_app.cpp`) is the canonical reference.

---

## 1. Choose the right base class

| Base | When to use |
|---|---|
| `ComponentApp` | A user-launchable app — appears in the app launcher, has an `id`, `name`, and `category` |
| `ComponentScreen` | A system screen (settings pages, modals, flows not exposed as apps) |

Both provide the same `build → layout → render` loop and identical input behaviour.
The difference is lifecycle: `ComponentApp::run()` is driven by `AppHost`; a
`ComponentScreen` is navigated to via `ViewDispatcher`.

---

## 2. Minimal boilerplate

### ComponentApp

```cpp
// hello_app.h
#pragma once
#include "nema/app/component_app.h"
#include "nema/ui/widgets.h"

namespace nema {

class HelloApp : public ComponentApp {
public:
    // IApp identity
    const char* id()       const override { return "palanu.hello"; }
    const char* name()     const override { return "Hello"; }
    const char* category() const override { return "Examples"; }

    aether::ui::UiNode* build(aether::ui::NodeArena& a, AppContext& ctx) override;

    void flipToggle() { toggleOn_ = !toggleOn_; }

private:
    aether::ui::ScrollState scroll_;
    bool toggleOn_ = false;
};

} // namespace nema
```

### ComponentScreen

```cpp
// my_screen.h
#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class MyScreen : public ComponentScreen {
public:
    explicit MyScreen(Runtime& rt) : ComponentScreen(rt, 256) {}

    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState scroll_;
};

} // namespace nema
```

---

## 3. The build() contract

`build()` is called once per frame when a redraw is needed. The arena is reset before
each call — every `UiNode*` you received last frame is gone.

Rules:
- **Do not** cache `UiNode*` across frames.
- **Do not** perform I/O, blocking waits, or side effects inside `build()`.
- **Do** read your own state (booleans, strings, vectors) and translate it into nodes.
- The returned root node must span the full available area (`flexGrow = 1`,
  `align = Align::Stretch`).

```cpp
aether::ui::UiNode* HelloApp::build(NodeArena& a, AppContext&) {
    Style root;
    root.dir      = FlexDir::Col;
    root.flexGrow = 1;
    root.align    = Align::Stretch;

    ListEntry settings; settings.label = "Settings"; settings.chevron = true;
    ListEntry about;    about.label    = "About";    about.value = "v1";

    return View(a, root, {
        TitleBar(a, "HELLO PALANU"),
        ListContainer(a, scroll_, {
            ListSection(a, "Menu"),
            ListItemRow(a, settings),
            ListItemRow(a, about),
            Toggle(a, "Dark mode", toggleOn_, onToggle, this),
        }),
    });
}
```

---

## 4. State mutation: do it in callbacks, not in build()

`build()` reads state; callbacks mutate it. The runtime redraws on the next tick.

```cpp
// Static callback — receives `this` as userdata
static void onToggle(void* u) {
    static_cast<HelloApp*>(u)->flipToggle();
}

void HelloApp::flipToggle() {
    toggleOn_ = !toggleOn_;
    // No explicit requestRedraw() needed inside ComponentApp — the base
    // redraws after any handled input.
}
```

For `ComponentScreen`, call `requestRedraw()` or set `dirty_ = true` after mutating
state in a callback, because the screen is not rebuilt automatically after every action.

---

## 5. ScrollState — one per scrollable list

Declare a `ScrollState` (or `VirtualListState`) as a member. It persists scroll
position across frames.

```cpp
// In the header:
aether::ui::ScrollState scroll_;

// In onResume() — reset scroll when entering the screen:
void MyScreen::onResume() {
    scroll_.scrollMain = 0;
    ComponentScreen::onResume();
}

// In build():
ListContainer(a, scroll_, { /* rows */ })
```

If you have multiple independent scrollable sections, declare one `ScrollState` per
section.

---

## 6. VirtualList — for large or dynamic lists

Use `VirtualList` when your list has more than ~20 items or when the arena overflows.

```cpp
// Header:
aether::ui::VirtualListState vlist_;
std::vector<std::string> items_;

// onResume:
void MyScreen::onResume() {
    loadItems();               // populate items_
    vlist_.scrollMain   = 0;
    vlist_.focusedIndex = 0;
    ComponentScreen::onResume();
}

// onAction — drive focus manually:
void MyScreen::onAction(input::Action a) {
    using A = input::Action;
    switch (a) {
        case A::Prev:     if (vlist_.moveFocus(-1)) requestRedraw(); break;
        case A::Next:     if (vlist_.moveFocus(+1)) requestRedraw(); break;
        case A::Activate: handleSelect(vlist_.focusedIndex); break;
        case A::Back:     rt_.view().goBack(); break;
        default: break;
    }
}

// build:
UiNode* MyScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    return View(a, root, {
        TitleBar(a, "My List"),
        VirtualList(a, vlist_, (int)items_.size(), 12, renderItem, this),
    });
}

// Static renderItem callback:
UiNode* MyScreen::renderItem(NodeArena& a, int i, bool focused, void* ud) {
    auto* self = static_cast<MyScreen*>(ud);
    ListEntry e; e.label = self->items_[i].c_str(); e.chevron = true;
    UiNode* row = ListItemRow(a, e);
    if (row) row->selfHighlight = focused;
    return row;
}
```

---

## 7. Logging

Use `rt_.log()` for all log output from screens, and `ctx.rt().log()` from apps
(or store `rt` as a member). Never use `printf`, `Serial`, or `ESP_LOGx`.

```cpp
rt_.log().info("MyScreen", "resumed");
rt_.log().info("MyScreen", "item selected", {{"index", std::to_string(i)}});
rt_.log().error("MyScreen", "load failed");
```

The component tag should be the class name, kept stable so logs are filterable.

---

## 8. Navigation

```cpp
// Push a new screen (forward):
rt_.view().navigate(nextScreen_, Transition::SlideLeft);

// Go back (pop):
rt_.view().goBack(Transition::SlideRight);

// Replace the current screen (no back-stack growth):
rt_.view().replace(otherScreen_);

// Pop all the way down to the base:
rt_.view().popToRoot();
```

Screens to navigate to are typically owned as members of the current screen or by a
higher-level manager (e.g. `SettingsScreen` owns all its sub-screens).

---

## 9. Do not do these things

| Don't | Why |
|---|---|
| `printf(...)` / `Serial.println(...)` in screens | Bypasses Logger sinks; violates observability rules |
| Hardcode `128`, `64`, or any screen dimension | Breaks on other display sizes; use `canvas.width()` / `canvas.height()` |
| Cache `UiNode*` across `build()` calls | The arena is reset; the pointer is stale |
| Call `rt_.view().navigate(...)` inside `build()` | Side effects in build cause re-entrant rebuild |
| Declare `ScrollState` as a local in `build()` | Scroll position is lost every frame |
| Use `#ifdef ESP32` or board-name checks in screen code | Violates the capability model; use `rt.capabilities().has(...)` |
| Log with `ESP_LOGx` | Bypasses MemorySink; Logs screen never sees it |

---

## Complete worked example: HelloApp

```cpp
// hello_app.h
#pragma once
#include "nema/app/component_app.h"
#include "nema/ui/widgets.h"

namespace nema {
class HelloApp : public ComponentApp {
public:
    const char* id()       const override { return "palanu.hello"; }
    const char* name()     const override { return "Hello"; }
    const char* category() const override { return "Examples"; }

    aether::ui::UiNode* build(aether::ui::NodeArena& a, AppContext& ctx) override;
    void flipToggle() { toggleOn_ = !toggleOn_; }

private:
    aether::ui::ScrollState scroll_;
    bool toggleOn_ = false;
};
} // namespace nema

// hello_app.cpp
#include "nema/apps/hello_app.h"
#include "nema/ui/widgets.h"
#include "nema/app/app_context.h"

namespace nema {
using namespace aether::ui;

static void onToggle(void* u) { static_cast<HelloApp*>(u)->flipToggle(); }

UiNode* HelloApp::build(NodeArena& a, AppContext&) {
    Style root;
    root.dir      = FlexDir::Col;
    root.flexGrow = 1;
    root.align    = Align::Stretch;

    ListEntry settings; settings.label = "Settings"; settings.chevron = true;
    ListEntry about;    about.label    = "About";    about.value = "v1";

    return View(a, root, {
        TitleBar(a, "HELLO PALANU"),
        ListContainer(a, scroll_, {
            ListSection(a, "Menu"),
            ListItemRow(a, settings),
            ListItemRow(a, about),
            Toggle(a, "Dark mode", toggleOn_, onToggle, this),
        }),
    });
}
} // namespace nema
```

Key observations from HelloApp:
- `scroll_` is a member — scroll position survives rebuilds.
- `toggleOn_` is a plain `bool` member — `build()` reads it; `flipToggle()` mutates it.
- The callback is a `static` free function that casts `userdata` back to `HelloApp*`.
- No coordinate math, no manual Canvas calls — the layout engine handles everything.
- The root `View` sets `flexGrow = 1` and `align = Stretch` so the tree fills the
  available area left by the status bar.
