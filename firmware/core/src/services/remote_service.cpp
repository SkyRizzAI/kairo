#include "nema/services/remote_service.h"
#include "nema/services/cli_service.h"
#include "nema/services/input_service.h"
#include "nema/hal/filesystem.h"
#include "nema/hal/ota.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/ui/key.h"
#include "nema/crypto/sha256.h"
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>

namespace nema {

void RemoteService::init(LinkService& link, InputService& input) {
    link_ = &link;
    input_ = &input;
    logSink_.link = &link;
    link_->onFrame(&RemoteService::onFrameThunk, this);
    link_->onReady(&RemoteService::onReadyThunk, this);            // handshake → auth
    link_->onHandshakeGate(&RemoteService::gateThunk, this);       // refuse if disabled
    link_->onDisconnect(&RemoteService::onDisconnectThunk, this);  // drop all sessions
}

bool RemoteService::gateThunk(void* user) {
    auto* self = static_cast<RemoteService*>(user);
    return !self->auth_ || self->auth_->enabled();   // Remote master switch
}

void RemoteService::onDisconnectThunk(void* user) {
    auto* self = static_cast<RemoteService*>(user);
    if (self->log_) self->log_->warn("Remote", "session disconnected (liveness/transport)");
    // Runs on the disconnect/RemoteToggled thread — serialise against the RX task's
    // auth handling (R1).
    std::lock_guard<std::mutex> lk(self->stateMtx_);
    if (self->sessions_) self->sessions_->clear();   // tear down every shell session
    // Release a half-finished streaming write so its file handle isn't leaked.
    if (self->xfer_.active && self->xfer_.streaming && self->fs_) self->fs_->writeStreamAbort();
    self->xfer_ = FileXfer{};
    self->nonce_.clear();
    // Re-lock for the next session (unless there's no password = always open).
    self->authorized_ = !(self->auth_ && self->auth_->hasPassword());
    if (self->link_) self->link_->setAuthorized(self->authorized_.load());
}

void RemoteService::onReadyThunk(void* user) {
    auto* self = static_cast<RemoteService*>(user);
    self->startAuth();
    if (self->readyFn_) self->readyFn_(self->readyUser_);   // platform screen-push
}

void RemoteService::sendCtrl(uint8_t op, const uint8_t* data, size_t len) {
    if (!link_) return;
    std::vector<uint8_t> buf;
    buf.reserve(1 + len);
    buf.push_back(op);
    if (data && len) buf.insert(buf.end(), data, data + len);
    link_->send(plp::Channel::Control, buf.data(), buf.size());
}

// Handshake just completed. With a password set, challenge the host; otherwise
// privileged channels stay open (backward-compatible no-auth default).
void RemoteService::startAuth() {
    if (auth_ && auth_->hasPassword()) {
        authorized_ = false;
        if (link_) link_->setAuthorized(false);          // blank the screen-tap pre-auth
        std::string ch;
        {
            std::lock_guard<std::mutex> lk(stateMtx_);
            nonce_ = randomHexSalt(8);
            ch = auth_->salt() + ":" + nonce_;            // "salt:nonce"
        }
        sendCtrl(LinkService::AUTH_CHALLENGE, (const uint8_t*)ch.data(), ch.size());
    } else {
        authorized_ = true;
        if (link_) link_->setAuthorized(true);
        // Explicit AUTH_OK with empty token: signals to the host that no password
        // is required so it can safely start file/system operations immediately.
        // Without this, hosts that await 'authorized' would time out on open devices.
        sendCtrl(LinkService::AUTH_OK, nullptr, 0);
    }
}

void RemoteService::handleAuthControl(const plp::Frame& f) {
    if (f.payload.empty() || !auth_) return;
    if (f.payload[0] != LinkService::AUTH_RESPONSE) return;   // device only consumes responses
    if (f.payload.size() < 2) { sendCtrl(LinkService::AUTH_FAIL, nullptr, 0); return; }
    char kind = (char)f.payload[1];
    std::string value((const char*)f.payload.data() + 2, f.payload.size() - 2);
    bool ok;
    {
        std::lock_guard<std::mutex> lk(stateMtx_);
        ok = (kind == 'T') ? auth_->validateToken(value)
                           : auth_->verify(nonce_, value);
    }
    if (ok) {
        authorized_ = true;
        std::string token = (kind == 'T') ? value : auth_->issueToken();
        sendCtrl(LinkService::AUTH_OK, (const uint8_t*)token.data(), token.size());
        if (link_) link_->setAuthorized(true);           // unblank: screen-tap resumes
        if (readyFn_) readyFn_(readyUser_);              // re-push the current screen now
    } else {
        sendCtrl(LinkService::AUTH_FAIL, nullptr, 0);
    }
}

// Send a CLI-channel frame for session `sid`: payload = [sid][text]. (Plan 45)
void RemoteService::sendCli(uint8_t sid, const std::string& text) {
    if (!link_) return;
    std::vector<uint8_t> buf;
    buf.reserve(text.size() + 1);
    buf.push_back(sid);
    buf.insert(buf.end(), text.begin(), text.end());
    link_->send(plp::Channel::Cli, buf.data(), buf.size());
}

void RemoteService::attachLog(Logger& log) {
    log_ = &log;
    log.addSink(logSink_);
}

void RemoteService::attachEvents(EventBus& bus) {
    events_ = &bus;
    // Remote master switch turned off → drop the live session immediately so the
    // screen-tap stops mirroring (inbound is already gated in dispatch()).
    bus.subscribe(events::RemoteToggled, [this](const Event&) {
        if (auth_ && !auth_->enabled() && link_) {
            link_->setAuthorized(false);
            link_->markDisconnected();
        }
    });
    // Stream every event to the host EVENT channel: [name\0][key\0val\0]...
    bus.subscribe("*", [this](const Event& e) {
        if (!link_ || !link_->ready()) return;
        std::vector<uint8_t> p;
        const char* n = e.name ? e.name : "";
        p.insert(p.end(), n, n + std::strlen(n));
        p.push_back(0);
        for (const auto& f : e.payload) {
            const char* k = f.key ? f.key : "";
            p.insert(p.end(), k, k + std::strlen(k));
            p.push_back(0);
            p.insert(p.end(), f.value.begin(), f.value.end());
            p.push_back(0);
        }
        link_->send(plp::Channel::Event, p.data(), p.size());
    });
}

void RemoteService::onFrameThunk(void* user, const plp::Frame& f) {
    static_cast<RemoteService*>(user)->dispatch(f);
}

void RemoteService::dispatch(const plp::Frame& f) {
    // Remote master switch (Plan 74): when disabled, ignore everything.
    if (auth_ && !auth_->enabled()) return;
    plp::Channel ch = (plp::Channel)f.channel;
    // Session auth handshake (Control channel) is always processed.
    if (ch == plp::Channel::Control) { handleAuthControl(f); return; }
    // Until authorized, drop ALL inbound app channels (input/system/cli/file/ota/ext).
    // With a password set this is what makes the device un-controllable pre-auth;
    // the outbound screen-tap is blanked in parallel by LinkService::send().
    if (!authorized_) {
        sendCtrl(LinkService::AUTH_REQUIRED, nullptr, 0);
        return;
    }
    switch (ch) {
        case plp::Channel::Input:
            if (!f.payload.empty() && input_)
                input_->post((Key)f.payload[0]);
            break;
        case plp::Channel::System:
            if (f.payload.empty()) break;
            if (f.payload[0] == SysOp::GetInfo) {
                // Reply [opcode][board-profile json] so the host can multiplex
                // future SYSTEM replies by the first byte.
                std::vector<uint8_t> p;
                p.reserve(1 + info_.size());
                p.push_back(SysOp::GetInfo);
                p.insert(p.end(), info_.begin(), info_.end());
                link_->send(plp::Channel::System, p.data(), p.size());
            } else if (f.payload[0] == SysOp::ScreenStream) {
                // Host opts in/out of the screen mirror. Default OFF per session, so
                // file/CLI tools don't get flooded; Forge web /remote turns it on.
                link_->setScreenWanted(f.payload.size() > 1 && f.payload[1] != 0);
            } else if (f.payload[0] >= SysOp::Restart && powerFn_) {
                powerFn_(powerUser_, f.payload[0]);
            }
            break;
        case plp::Channel::Cli: {            // host→device terminal: [sid][command line]
            if (!cli_ || !sessions_ || f.payload.empty()) break;
            uint8_t sid = f.payload[0];      // session id (Plan 45) — isolates shells
            std::string line((const char*)f.payload.data() + 1, f.payload.size() - 1);
            CliSession& s = sessions_->get(sid);
            if (!s.out) s.out = [this, sid](const std::string& t) { sendCli(sid, t); };
            cli_->execute(line, s);          // per-session: cwd + history
            // Prompt update (Plan 44 Fase 4): [0x01]<cwd> so the host shows cwd.
            sendCli(sid, std::string(1, (char)0x01) + s.cwd);
            const uint8_t eot[2] = {sid, 0x04};   // end-of-output for this session
            link_->send(plp::Channel::Cli, eot, 2);
            break;
        }
        case plp::Channel::File:             // host→device filesystem request
            // Handled inline on the receiving task (cdc_rx on ESP32, page-thread on
            // WASM). The chunked write protocol keeps each frame cheap (see Plan 88
            // §17 / ADR 0009); the old async-deferral seam was removed as a footgun.
            if (fs_ && !f.payload.empty()) handleFile(f.payload);
            break;
        case plp::Channel::Ota:              // host→device firmware push (Plan 39)
            if (!f.payload.empty()) handleOta(f.payload);
            break;
        case plp::Channel::Ext: {            // host→device sim control
            if (f.payload.empty()) break;
            uint8_t op = f.payload[0];
            if (op == ExtOp::InjectEvent && events_) {
                // payload: [op][eventName\0]
                std::string name((const char*)f.payload.data() + 1);
                if (!name.empty()) events_->publish({name.c_str(), {}});
            } else if (controlFn_) {
                controlFn_(controlUser_, op, f.payload.data() + 1, f.payload.size() - 1);
            }
            break;
        }
        default:
            break;   // SCREEN/LOG/OTA handled elsewhere or outbound-only
    }
}

void RemoteService::handleOta(const std::vector<uint8_t>& in) {
    const uint8_t op = in[0];
    // Reply on the OTA channel: [op][status]([written:4 LE]).
    auto reply = [&](uint8_t status, uint32_t written = 0) {
        uint8_t p[6] = {op, status,
                        (uint8_t)written, (uint8_t)(written >> 8),
                        (uint8_t)(written >> 16), (uint8_t)(written >> 24)};
        link_->send(plp::Channel::Ota, p, 6);
    };
    if (!ota_ || !ota_->supported()) { reply(OtaStatus::Unsupported); return; }

    switch (op) {
        case OtaOp::Begin: {
            uint32_t size = 0;   // [op][size:4 LE] (0 = unknown)
            if (in.size() >= 5)
                size = in[1] | (in[2] << 8) | (in[3] << 16) | ((uint32_t)in[4] << 24);
            bool ok = ota_->begin(size);
            // Begin reply is extended: [op][status][written:4][protoVersion]. The
            // host checks protoVersion to catch a stale-firmware mismatch.
            uint8_t p[7] = {op, ok ? OtaStatus::Ok : OtaStatus::Error, 0, 0, 0, 0, OtaProtoVersion};
            link_->send(plp::Channel::Ota, p, 7);
            break;
        }
        case OtaOp::Data: {
            // [op][offset:4 LE][bytes]. Idempotent: dedupe a resent chunk by offset
            // so a retry (after a dropped frame / lost ack) never double-writes.
            if (in.size() < 5) { reply(OtaStatus::Error, ota_->written()); break; }
            uint32_t off  = in[1] | (in[2] << 8) | (in[3] << 16) | ((uint32_t)in[4] << 24);
            uint32_t have = ota_->written();
            if (off == have) {
                bool ok = ota_->write(in.data() + 5, in.size() - 5);
                reply(ok ? OtaStatus::Ok : OtaStatus::Error, ota_->written());
            } else if (off < have) {
                reply(OtaStatus::Ok, have);     // already written (retry) — ack, no rewrite
            } else {
                reply(OtaStatus::Error, have);  // gap — host resyncs from `written`
            }
            break;
        }
        case OtaOp::End:
            if (ota_->commit()) {
                reply(OtaStatus::Ok);
                // Boot into the freshly-written slot — but ONLY for a real updater.
                // The WASM dry-run returns false here: a "restart" there just halts
                // the in-browser device (no auto-reload) and would wedge the session.
                if (powerFn_ && ota_->rebootOnCommit()) powerFn_(powerUser_, SysOp::Restart);
            } else {
                reply(OtaStatus::Error);
            }
            break;
        case OtaOp::Abort:
            ota_->abort();
            reply(OtaStatus::Ok);
            break;
        default:
            reply(OtaStatus::Error);
            break;
    }
}

// FILE v2 (Plan 88): the device's per-chunk acks dictate this size to the host,
// kept well under the USB-CDC RX ring so a write burst never overruns it (a single
// whole-file frame did — the root cause of the `cp` failure).
static constexpr uint16_t kFileChunk = 1024;

void RemoteService::handleFile(const std::vector<uint8_t>& in) {
    // Every FILE request is [op][reqId:2][op-specific...].
    if (in.size() < 3) return;
    const uint8_t  op    = in[0];
    const uint16_t reqId = (uint16_t)(in[1] | (in[2] << 8));
    const uint8_t* a     = in.data() + 3;          // args after the envelope
    const size_t   alen  = in.size() - 3;

    // Reply framing: [op][reqId:2][status][extra...]. Correlated by reqId, so no
    // path echo is needed (v1 echoed the path purely for the client to skip it).
    auto reply = [&](uint8_t status, const uint8_t* extra, size_t extraLen) {
        std::vector<uint8_t> p;
        p.reserve(4 + extraLen);
        p.push_back(op);
        p.push_back((uint8_t)(reqId & 0xff));
        p.push_back((uint8_t)((reqId >> 8) & 0xff));
        p.push_back(status);
        if (extra && extraLen) p.insert(p.end(), extra, extra + extraLen);
        link_->send(plp::Channel::File, p.data(), p.size());
    };

    switch (op) {
        // ── Chunked write (host→device), paced by acks ──
        // WriteBegin starts a fresh transfer and returns its xferId; WriteData/WriteEnd
        // carry that xferId (NOT the per-message reqId, which differs every frame).
        case FileOp::WriteBegin: {
            if (alen < 4) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            uint32_t total = (uint32_t)a[0] | ((uint32_t)a[1] << 8) |
                             ((uint32_t)a[2] << 16) | ((uint32_t)a[3] << 24);
            if (++xferCounter_ == 0) xferCounter_ = 1;   // never 0 (0 = "none")
            xfer_.active = true;
            xfer_.xferId = xferCounter_;
            xfer_.written = 0;
            xfer_.path.assign((const char*)a + 4, alen - 4);
            // Prefer streaming straight to disk (no whole-file RAM buffer — R5). Falls
            // back to a RAM buffer if the backend can't stream (e.g. WASM MemFS).
            xfer_.streaming = fs_ && fs_->writeStreamBegin(xfer_.path);
            if (!xfer_.streaming) {
                xfer_.buf.clear();
                if (total && total < (4u << 20)) xfer_.buf.reserve(total);
            }
            uint8_t ack[4] = { (uint8_t)(kFileChunk & 0xff), (uint8_t)((kFileChunk >> 8) & 0xff),
                               (uint8_t)(xfer_.xferId & 0xff), (uint8_t)((xfer_.xferId >> 8) & 0xff) };
            reply(FileStatus::Ok, ack, 4);   // [chunkSize:2][xferId:2]
            break;
        }
        case FileOp::WriteData: {
            // [xferId:2][offset:4][bytes] — reject a chunk that isn't part of the active
            // transfer (N2): protects an in-flight write from a second client.
            if (alen < 6 || !xfer_.active) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            uint16_t xid = (uint16_t)(a[0] | (a[1] << 8));
            if (xid != xfer_.xferId) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            uint32_t off = (uint32_t)a[2] | ((uint32_t)a[3] << 8) |
                           ((uint32_t)a[4] << 16) | ((uint32_t)a[5] << 24);
            const uint8_t* bytes = a + 6;
            size_t blen = alen - 6;
            uint32_t next;
            if (xfer_.streaming) {
                // Seek+write straight to disk; offset makes a retried chunk idempotent.
                errno = 0;
                if (!fs_->writeStreamChunk(off, bytes, blen)) {
                    if (log_) log_->warn("RemoteFile", "stream chunk failed",
                                         {{"off", std::to_string(off)}, {"len", std::to_string(blen)}, {"errno", std::to_string(errno)}});
                    fs_->writeStreamAbort();
                    xfer_ = FileXfer{};
                    reply(FileStatus::Error, nullptr, 0);
                    break;
                }
                if (off + blen > xfer_.written) xfer_.written = off + (uint32_t)blen;
                next = xfer_.written;
            } else {
                const size_t have = xfer_.buf.size();
                if (off == have) {
                    xfer_.buf.insert(xfer_.buf.end(), bytes, bytes + blen);   // in-order append
                } else if (off < have) {
                    // Duplicate/retried chunk (idempotent). Append only the tail we lack.
                    size_t overlap = have - off;
                    if (blen > overlap) xfer_.buf.insert(xfer_.buf.end(), bytes + overlap, bytes + blen);
                } else {
                    reply(FileStatus::BadRequest, nullptr, 0);               // gap → host must resend
                    break;
                }
                next = (uint32_t)xfer_.buf.size();
            }
            uint8_t ack[4] = { (uint8_t)(next & 0xff), (uint8_t)((next >> 8) & 0xff),
                               (uint8_t)((next >> 16) & 0xff), (uint8_t)((next >> 24) & 0xff) };
            reply(FileStatus::Ok, ack, 4);
            break;
        }
        case FileOp::WriteEnd: {
            // [xferId:2]. Idempotent retry: a re-sent WriteEnd (its ack was lost)
            // replays the stored result once xfer_ is already torn down (N3).
            if (alen < 2) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            uint16_t xid = (uint16_t)(a[0] | (a[1] << 8));
            if (!xfer_.active) {
                uint8_t st = (xid != 0 && xid == lastEndXferId_) ? lastEndStatus_
                                                                 : FileStatus::BadRequest;
                reply(st, nullptr, 0);
                break;
            }
            if (xid != xfer_.xferId) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            bool ok;
            int  e = 0;
            if (xfer_.streaming) {
                ok = fs_->writeStreamEnd();
                e  = errno;
            } else {
                size_t n = xfer_.buf.size();
                // 0-byte file: pass a valid (non-null) pointer; an empty vector's data()
                // may be nullptr and some backends reject (data==null) outright (N5).
                static const uint8_t kEmpty = 0;
                const uint8_t* d = n ? xfer_.buf.data() : &kEmpty;
                errno = 0;
                ok = fs_ && fs_->write(xfer_.path, d, n);
                e  = errno;   // best-effort: backends use fopen/fwrite which set errno
            }
            uint8_t st = ok ? FileStatus::Ok : FileStatus::fromErrno(e);
            if (!ok && log_)
                log_->warn("RemoteFile", "write failed", {{"path", xfer_.path}, {"errno", std::to_string(e)}});
            lastEndXferId_ = xfer_.xferId;                              // remember for retry
            lastEndStatus_ = st;
            xfer_ = FileXfer{};                                         // release the buffer
            reply(st, nullptr, 0);
            break;
        }

        // ── Two-path ops: [reqId][srcLen:2 LE][src][dst] ──
        case FileOp::Rename:
        case FileOp::Copy: {
            if (alen < 2) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            uint16_t sl = (uint16_t)(a[0] | (a[1] << 8));
            if (alen < (size_t)2 + sl) { reply(FileStatus::BadRequest, nullptr, 0); break; }
            std::string src((const char*)a + 2, sl);
            std::string dst((const char*)a + 2 + sl, alen - 2 - sl);
            bool ok;
            if (op == FileOp::Rename) {
                ok = fs_ && fs_->rename(src, dst);
            } else {  // Copy: read src → write dst
                std::vector<uint8_t> data;
                ok = fs_ && fs_->read(src, data) && fs_->write(dst, data.data(), data.size());
            }
            reply(ok ? FileStatus::Ok : FileStatus::Error, nullptr, 0);
            break;
        }

        // ── Single-path ops: [reqId][path] ──
        case FileOp::List: {
            std::string path((const char*)a, alen);
            std::vector<FsEntry> entries;
            bool ok = fs_ && fs_->list(path, entries);
            std::vector<uint8_t> body;
            for (const auto& e : entries) {           // [type:1][size:4 LE][name\0]
                body.push_back(e.isDir ? 1 : 0);
                body.push_back(e.size & 0xff);
                body.push_back((e.size >> 8) & 0xff);
                body.push_back((e.size >> 16) & 0xff);
                body.push_back((e.size >> 24) & 0xff);
                body.insert(body.end(), e.name.begin(), e.name.end());
                body.push_back(0);
            }
            reply(ok ? FileStatus::Ok : FileStatus::NotFound, body.data(), body.size());
            break;
        }
        case FileOp::Read: {
            std::string path((const char*)a, alen);
            std::vector<uint8_t> data;
            bool ok = fs_ && fs_->read(path, data);
            reply(ok ? FileStatus::Ok : FileStatus::NotFound, data.data(), data.size());
            break;
        }
        case FileOp::ReadBegin: {
            // Chunked read: ack with total size, then push ReadData frames, then
            // ReadEnd. Device→host, so a burst is fine (no RX-ring limit on the host).
            std::string path((const char*)a, alen);
            std::vector<uint8_t> data;
            bool ok = fs_ && fs_->read(path, data);
            uint32_t total = ok ? (uint32_t)data.size() : 0;
            uint8_t hdr[6] = { (uint8_t)(total & 0xff), (uint8_t)((total >> 8) & 0xff),
                               (uint8_t)((total >> 16) & 0xff), (uint8_t)((total >> 24) & 0xff),
                               (uint8_t)(kFileChunk & 0xff), (uint8_t)((kFileChunk >> 8) & 0xff) };
            reply(ok ? FileStatus::Ok : FileStatus::NotFound, hdr, 6);  // op = ReadBegin
            if (!ok) break;
            // Helper: send [op][reqId][payload] for the streamed frames.
            auto stream = [&](uint8_t sop, const uint8_t* p, size_t plen) {
                std::vector<uint8_t> f;
                f.reserve(3 + plen);
                f.push_back(sop);
                f.push_back((uint8_t)(reqId & 0xff));
                f.push_back((uint8_t)((reqId >> 8) & 0xff));
                if (p && plen) f.insert(f.end(), p, p + plen);
                link_->send(plp::Channel::File, f.data(), f.size());
            };
            for (uint32_t off = 0; off < total; off += kFileChunk) {
                uint32_t n = total - off < kFileChunk ? total - off : kFileChunk;
                std::vector<uint8_t> body;
                body.reserve(4 + n);
                body.push_back((uint8_t)(off & 0xff));
                body.push_back((uint8_t)((off >> 8) & 0xff));
                body.push_back((uint8_t)((off >> 16) & 0xff));
                body.push_back((uint8_t)((off >> 24) & 0xff));
                body.insert(body.end(), data.begin() + off, data.begin() + off + n);
                stream(FileOp::ReadData, body.data(), body.size());
            }
            uint8_t st = FileStatus::Ok;
            stream(FileOp::ReadEnd, &st, 1);
            break;
        }
        case FileOp::Mkdir: {
            std::string path((const char*)a, alen);
            reply(fs_ && fs_->mkdir(path) ? FileStatus::Ok : FileStatus::Error, nullptr, 0);
            break;
        }
        case FileOp::Remove: {
            std::string path((const char*)a, alen);
            reply(fs_ && fs_->remove(path) ? FileStatus::Ok : FileStatus::Error, nullptr, 0);
            break;
        }
        default:
            reply(FileStatus::BadRequest, nullptr, 0);
            break;
    }
}

void RemoteService::LinkLogSink::write(const LogEntry& e) {
    if (!link || !link->ready()) return;
    std::vector<uint8_t> p;
    p.push_back((uint8_t)e.level);
    const char* c = e.component ? e.component : "";
    p.insert(p.end(), c, c + std::strlen(c));
    p.push_back(0);
    p.insert(p.end(), e.message.begin(), e.message.end());
    p.push_back(0);
    link->send(plp::Channel::Log, p.data(), p.size());
}

} // namespace nema
