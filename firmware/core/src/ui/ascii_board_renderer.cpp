#include "nema/ui/ascii_board_renderer.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace nema::ui {

namespace {

static int roundF(float f) {
    return static_cast<int>(f + 0.5f);
}

static int clampI(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void drawBox(std::vector<std::string>& grid, int x, int y, int w, int h) {
    if (w < 2 || h < 2) return;
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    int x0 = clampI(x, 0, cols - 1);
    int y0 = clampI(y, 0, rows - 1);
    int x1 = clampI(x + w - 1, 0, cols - 1);
    int y1 = clampI(y + h - 1, 0, rows - 1);

    for (int row = y0; row <= y1; ++row) {
        for (int col = x0; col <= x1; ++col) {
            if (row == y0 && col == x0)      grid[row][col] = '+';
            else if (row == y0 && col == x1) grid[row][col] = '+';
            else if (row == y1 && col == x0) grid[row][col] = '+';
            else if (row == y1 && col == x1) grid[row][col] = '+';
            else if (row == y0 || row == y1) grid[row][col] = '-';
            else if (col == x0 || col == x1) grid[row][col] = '|';
            else                              grid[row][col] = ' ';
        }
    }
}

static void drawLabel(std::vector<std::string>& grid, int x, int y, int w, int h, const char* label) {
    if (!label || w < 1 || h < 1) return;
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    int len = static_cast<int>(std::strlen(label));
    int cx = x + (w - len) / 2;
    int cy = y + h / 2;
    if (cy < 0 || cy >= rows) return;
    for (int i = 0; i < len; ++i) {
        int col = cx + i;
        if (col >= 0 && col < cols) {
            grid[cy][col] = label[i];
        }
    }
}

static void drawButtonNum(std::vector<std::string>& grid, int x, int y, uint8_t id) {
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    // Draw [N] border around button number
    char num = (id < 10) ? ('0' + id) : '?';
    if (x >= 0 && x + 2 < cols && y >= 0 && y < rows) {
        grid[y][x]     = '[';
        grid[y][x + 1] = num;
        grid[y][x + 2] = ']';
    } else if (x >= 0 && x + 1 < cols && y >= 0 && y < rows) {
        grid[y][x]     = num;
    }
}

} // anonymous namespace

std::vector<std::string> AsciiBoardRenderer::render(
    const BoardProfile& profile,
    uint8_t cols,
    uint8_t rows
) {
    std::vector<std::string> grid(rows, std::string(cols, ' '));

    float boardAspect = profile.board_w / profile.board_h;
    float gridAspect  = static_cast<float>(cols) / static_cast<float>(rows);

    int ew, eh, ox, oy;
    if (boardAspect > gridAspect) {
        ew = cols;
        eh = roundF(static_cast<float>(cols) / boardAspect);
        ox = 0;
        oy = (rows - eh) / 2;
    } else {
        eh = rows;
        ew = roundF(static_cast<float>(rows) * boardAspect);
        ox = (cols - ew) / 2;
        oy = 0;
    }

    for (uint8_t i = 0; i < profile.component_count; ++i) {
        const auto& c = profile.components[i];
        int gx = ox + roundF(c.x * ew);
        int gy = oy + roundF(c.y * eh);
        int gw = std::max(1, roundF(c.w * ew));
        int gh = std::max(1, roundF(c.h * eh));

        switch (c.type) {
            case ComponentType::Display:
                drawBox(grid, gx, gy, gw, gh);
                drawLabel(grid, gx, gy, gw, gh, c.label);
                break;
            case ComponentType::Button:
                drawButtonNum(grid, gx, gy, c.id);
                break;
            default:
                drawButtonNum(grid, gx, gy, c.id);
                break;
        }
    }

    return grid;
}

std::vector<std::string> AsciiBoardRenderer::renderLegend(
    const BoardProfile& profile
) {
    std::vector<std::string> lines;
    for (uint8_t i = 0; i < profile.component_count; ++i) {
        const auto& c = profile.components[i];
        if (c.type != ComponentType::Button) continue;

        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d %s", c.id, c.label);
        lines.push_back(buf);
    }
    return lines;
}

} // namespace nema::ui
