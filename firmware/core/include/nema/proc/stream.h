#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// Plan 54 — Stream primitives for process stdio + pipe.
// Minimal byte-stream abstraction: read() → non-blocking-friendly, write() → sink.
// Runtime-agnostic: works for native C++, WASM (WASI fd), and JS (EventEmitter).

namespace nema {

// ── IInputStream — byte source (stdin / read-end of pipe) ─────────────────

struct IInputStream {
    virtual ~IInputStream() = default;

    // Read up to n bytes. Returns:
    //   > 0   bytes read into buf
    //   = 0   EOF (writer closed)
    //   < 0   no data available right now (would-block); try again later
    virtual int read(uint8_t* buf, size_t n) = 0;

    // Has the writer closed the stream? Once true, stays true.
    virtual bool eof() const = 0;

    // Convenience: read until '\n' or EOF. Returns false on EOF with nothing read.
    bool readLine(std::string& out);
};

// ── IOutputStream — byte sink (stdout / stderr / write-end of pipe) ───────

struct IOutputStream {
    virtual ~IOutputStream() = default;

    virtual void write(const uint8_t* buf, size_t n) = 0;

    void writeStr(const std::string& s) {
        write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    virtual void flush() {}
    virtual void close() {}   // close write-end → reader sees EOF
};

// ── In-memory stream adapters ─────────────────────────────────────────────

// An input stream that is always at EOF (no data, no producer). Used as
// default stdin for on-device apps that don't pipe.
class NullInputStream : public IInputStream {
public:
    int  read(uint8_t*, size_t) override { return 0; }
    bool eof() const override { return true; }
};

// Wraps a std::string as a read-once input stream (for testing / argv parsing).
class StringInputStream : public IInputStream {
public:
    explicit StringInputStream(std::string data) : data_(std::move(data)) {}

    int read(uint8_t* buf, size_t n) override {
        if (pos_ >= data_.size()) return 0;
        size_t m = std::min(n, data_.size() - pos_);
        std::memcpy(buf, data_.data() + pos_, m);
        pos_ += m;
        return static_cast<int>(m);
    }
    bool eof() const override { return pos_ >= data_.size(); }

private:
    std::string data_;
    size_t pos_ = 0;
};

// Captures all writes into a std::string (for testing / output capture).
class StringOutputStream : public IOutputStream {
public:
    void write(const uint8_t* buf, size_t n) override {
        data_.append(reinterpret_cast<const char*>(buf), n);
    }
    const std::string& str() const { return data_; }

private:
    std::string data_;
};

// Adapter: writes to a CliSession-style line-based output function.
// Buffers bytes until '\n', then calls the output function per line.
class LineOutputStream : public IOutputStream {
public:
    using LineOut = std::function<void(const std::string&)>;

    explicit LineOutputStream(LineOut out) : out_(std::move(out)) {}

    void write(const uint8_t* buf, size_t n) override {
        for (size_t i = 0; i < n; i++) {
            char ch = static_cast<char>(buf[i]);
            if (ch == '\n') {
                out_(buf_);
                buf_.clear();
            } else {
                buf_ += ch;
            }
        }
    }
    void flush() override {
        if (!buf_.empty()) { out_(buf_); buf_.clear(); }
    }
    void close() override { flush(); }

private:
    LineOut     out_;
    std::string buf_;
};

} // namespace nema
