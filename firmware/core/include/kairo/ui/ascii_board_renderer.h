#pragma once
#include "kairo/system/board_profile.h"
#include <vector>
#include <string>

namespace kairo::ui {

class AsciiBoardRenderer {
public:
    static std::vector<std::string> render(
        const BoardProfile& profile,
        uint8_t cols,
        uint8_t rows
    );

    static std::vector<std::string> renderLegend(
        const BoardProfile& profile
    );
};

} // namespace kairo::ui
