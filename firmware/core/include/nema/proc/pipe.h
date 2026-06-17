#pragma once
#include "nema/proc/stream.h"
#include <cstdint>
#include <cstring>
#include <vector>

// Plan 54 — SPSC Pipe (single producer, single consumer).
// Wraps a fixed-capacity ring buffer: write-end → ring → read-end.
// writer.close() → reader sees EOF after draining remaining bytes.

namespace nema {

class Pipe {
public:
    explicit Pipe(size_t capacity = 2048);

    IOutputStream& writer();   // process A's stdout
    IInputStream&  reader();   // process B's stdin

private:
    class WriteEnd : public IOutputStream {
    public:
        explicit WriteEnd(Pipe& p) : pipe_(p) {}
        void write(const uint8_t* buf, size_t n) override;
        void close() override { closed_ = true; }

    private:
        friend class Pipe;
        Pipe& pipe_;
        bool  closed_ = false;
    };

    class ReadEnd : public IInputStream {
    public:
        explicit ReadEnd(Pipe& p) : pipe_(p) {}
        int  read(uint8_t* buf, size_t n) override;
        bool eof() const override { return eof_; }

    private:
        friend class Pipe;
        Pipe& pipe_;
        bool  eof_ = false;
    };

    std::vector<uint8_t> buf_;
    size_t               writePos_ = 0;
    size_t               readPos_  = 0;
    size_t               count_    = 0;   // bytes in buffer
    bool                 writerClosed_ = false;

    WriteEnd writeEnd_{*this};
    ReadEnd  readEnd_{*this};
};

} // namespace nema
