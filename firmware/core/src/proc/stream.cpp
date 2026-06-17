// Plan 54 — Stream helper implementations.
#include "nema/proc/stream.h"
#include <algorithm>
#include <cstring>

namespace nema {

bool IInputStream::readLine(std::string& out) {
    out.clear();
    while (true) {
        uint8_t b;
        int n = read(&b, 1);
        if (n <= 0) return !out.empty();  // EOF: return true if we got something
        out += static_cast<char>(b);
        if (b == '\n') return true;
    }
}

} // namespace nema
