// Plan 54 — Pipe implementation.
#include "nema/proc/pipe.h"
#include <algorithm>

namespace nema {

Pipe::Pipe(size_t capacity) : buf_(capacity) {}

IOutputStream& Pipe::writer() { return writeEnd_; }
IInputStream&  Pipe::reader() { return readEnd_; }

void Pipe::WriteEnd::write(const uint8_t* data, size_t n) {
    if (closed_) return;
    size_t cap = pipe_.buf_.size();
    for (size_t i = 0; i < n; i++) {
        // Block / spin if buffer is full (would-block semantics).
        // In practice, producer spins until consumer drains; for now, drop.
        if (pipe_.count_ >= cap) break;
        pipe_.buf_[pipe_.writePos_] = data[i];
        pipe_.writePos_ = (pipe_.writePos_ + 1) % cap;
        pipe_.count_++;
    }
}

int Pipe::ReadEnd::read(uint8_t* buf, size_t n) {
    if (eof_) return 0;
    size_t cap = pipe_.buf_.size();

    if (pipe_.count_ == 0) {
        if (pipe_.writerClosed_ || pipe_.writeEnd_.closed_) {
            eof_ = true;
            return 0;  // EOF
        }
        return -1;  // would-block: no data, writer still open
    }

    size_t m = std::min(n, pipe_.count_);
    for (size_t i = 0; i < m; i++) {
        buf[i] = pipe_.buf_[pipe_.readPos_];
        pipe_.readPos_ = (pipe_.readPos_ + 1) % cap;
        pipe_.count_--;
    }
    return static_cast<int>(m);
}

} // namespace nema
