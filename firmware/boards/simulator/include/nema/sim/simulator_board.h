#pragma once
#include "nema/board.h"

namespace nema {

constexpr ComponentDef kSimComponents[] = {
    { 1, "Up",      ComponentType::Button,  0.13f, 0.72f, 0.07f, 0.07f, Key::Up     },
    { 2, "Left",    ComponentType::Button,  0.04f, 0.81f, 0.07f, 0.07f, Key::Left   },
    { 3, "Right",   ComponentType::Button,  0.22f, 0.81f, 0.07f, 0.07f, Key::Right  },
    { 4, "Down",    ComponentType::Button,  0.13f, 0.90f, 0.07f, 0.07f, Key::Down   },
    { 5, "Select",  ComponentType::Button,  0.70f, 0.78f, 0.10f, 0.07f, Key::Select },
    { 6, "Cancel",  ComponentType::Button,  0.83f, 0.90f, 0.10f, 0.07f, Key::Cancel },
    { 7, "LCD",     ComponentType::Display, 0.04f, 0.04f, 0.92f, 0.66f },
};

constexpr BoardProfile kSimProfile = {
    "simulator", "Palanu Simulator",
    90.0f, 55.0f,
    kSimComponents, 7
};

class SimulatorBoard : public IBoard {
public:
    const char* name() const override { return "simulator"; }
    void describeHardware(Runtime& rt) override;
    const BoardProfile& profile() const override { return kSimProfile; }
};

} // namespace nema
