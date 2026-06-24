# Aether UI — Migration Guide (Plan 90 patterns)

Quick reference for updating screen code to the current widget API. Before/after
patterns only — see `docs/feats/aether-ui.md` for full API reference.

---

## ListRow / ListItem → ListItemRow

**Before:**
```cpp
ListRow(a, label, value, icon)
ListItem(a, label)
```

**After:**
```cpp
ListEntry e;
e.label    = label;
e.value    = value;      // optional
e.leftIcon = icon;       // optional (uint8_t* XBM)
e.iconW    = 8; e.iconH = 8;
e.chevron  = true;       // optional
e.onPress  = myCallback; // optional
e.user     = this;
ListItemRow(a, e)
```

Fields not set keep their zero defaults and are ignored by the renderer.

---

## ListInputRow old params → ListInput struct

**Before:**
```cpp
ListInputRow(a, label, value, onPrev, onNext, ud)
```

**After:**
```cpp
ListInput in;
in.label    = label;
in.value    = value;
in.canPrev  = hasPrev;
in.canNext  = hasNext;
in.onAdjust = [](void* u, int dir) { ... };
in.user     = ud;
ListInputRow(a, in)
```

---

## Manual child-append → initializer list

**Before:**
```cpp
UiNode* list = ListContainer(a, scroll_, {});
UiNode* prev = nullptr;
for (auto& item : items_) {
    UiNode* row = ListItemRow(a, makeEntry(item));
    if (!prev) list->firstChild = row;
    else        prev->nextSibling = row;
    prev = row;
}
```

**After (static lists):**
```cpp
ListContainer(a, scroll_, {
    ListSection(a, "Section"),
    ListItemRow(a, e1),
    ListItemRow(a, e2),
    Toggle(a, "Option", on_, onToggle, this),
})
```

When the list is dynamic (items from a vector), the manual-append pattern is still
correct — use it when the initializer list cannot enumerate items at compile time.
The VirtualList is the better choice for dynamic lists over ~20 items.

---

## Modal screens → Dialog widget

**Before (a dedicated IScreen subclass pushed as a modal):**
```cpp
rt_.view().navigate(confirmModal_);
// confirmModal_.build() manually drew a box with buttons
```

**After (Dialog widget returned directly from build()):**
```cpp
// In the screen's build():
DialogButton btns[2] = {
    {"Confirm", onConfirm, this},
    {"Cancel",  onCancel,  this, false},
};
return Dialog(a, "Delete file?", "This cannot be undone.",
              nullptr, 0, 0, btns, 2);
```

Pass `danger = true` on a `DialogButton` to render it filled/inverted (for
destructive actions).

For in-app modals (overlaying an app's own content), override `buildModal()` in
`ComponentApp` and return a `Modal(a, { ... })` node instead.

---

## Hardcoded hint labels → hintFor()

**Before:**
```cpp
Footer(a, "[Back] Cancel  [Ok] Confirm")
```

**After:**
```cpp
Footer(a, rt_.input().hintFor(input::Action::Back))
```

Hardcoded button names are wrong on boards with different physical controls.
`hintFor()` returns the label string for the current board's key mapping.

---

## ComponentScreen build() signature

The build signature for `ComponentScreen` subclasses takes `Runtime&`, not `AppContext&`:

```cpp
// System screen (ComponentScreen):
aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

// App (ComponentApp):
aether::ui::UiNode* build(aether::ui::NodeArena& a, AppContext& ctx) override;
```

These are different base classes. Do not mix the signatures.

---

## When to use VirtualList vs ListContainer

| Condition | Use |
|---|---|
| Static or small list (< ~20 items) | `ListContainer` with initializer list or manual append |
| Dynamic list > ~20 items | `VirtualList` |
| Arena overflow occurring on a large list | `VirtualList` |
| Items need section headers between them | `ListContainer` (`ListSection` is not supported inside `VirtualList`) |

**Migrating a ListContainer to VirtualList:**

```cpp
// Before (ListContainer):
aether::ui::ScrollState scroll_;

UiNode* build(NodeArena& a, Runtime&) {
    return ListContainer(a, scroll_, {
        ListItemRow(a, makeEntry(items_[0])),
        // ... many rows
    });
}

// After (VirtualList):
aether::ui::VirtualListState vlist_;

void onAction(input::Action action) {
    if (action == input::Action::Prev) { if (vlist_.moveFocus(-1)) requestRedraw(); }
    if (action == input::Action::Next) { if (vlist_.moveFocus(+1)) requestRedraw(); }
    if (action == input::Action::Activate) { handleSelect(vlist_.focusedIndex); }
}

UiNode* build(NodeArena& a, Runtime&) {
    return VirtualList(a, vlist_, (int)items_.size(), 12, renderItem, this);
}

static UiNode* renderItem(NodeArena& a, int i, bool focused, void* ud) {
    auto* self = static_cast<MyScreen*>(ud);
    ListEntry e; e.label = self->items_[i].name.c_str(); e.chevron = true;
    UiNode* row = ListItemRow(a, e);
    if (row) row->selfHighlight = focused;
    return row;
}
```

Note: `VirtualListState` replaces `ScrollState` — it extends it. Reset focus and
scroll in `onResume()`:

```cpp
void onResume() {
    vlist_.scrollMain   = 0;
    vlist_.focusedIndex = 0;
    ComponentScreen::onResume();
}
```
