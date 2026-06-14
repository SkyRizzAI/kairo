#include "nema/services/remote_service.h"
#include "nema/services/cli_service.h"
#include "nema/services/input_service.h"
#include "nema/hal/filesystem.h"
#include "nema/hal/ota.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/ui/key.h"
#include <cstring>
#include <string>
#include <vector>

namespace nema {

void RemoteService::init(LinkService& link, InputService& input) {
    link_ = &link;
    input_ = &input;
    logSink_.link = &link;
    link_->onFrame(&RemoteService::onFrameThunk, this);
    link_->onDisconnect(&RemoteService::onDisconnectThunk, this);  // drop all sessions
}

void RemoteService::onDisconnectThunk(void* user) {
    auto* self = static_cast<RemoteService*>(user);
    if (self->sessions_) self->sessions_->clear();   // tear down every shell session
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
    log.addSink(logSink_);
}

void RemoteService::attachEvents(EventBus& bus) {
    events_ = &bus;
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
    switch ((plp::Channel)f.channel) {
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
            reply(ota_->begin(size) ? OtaStatus::Ok : OtaStatus::Error);
            break;
        }
        case OtaOp::Data:        // [op][chunk...] — ack with bytes-written for flow control
            reply(ota_->write(in.data() + 1, in.size() - 1) ? OtaStatus::Ok : OtaStatus::Error,
                  ota_->written());
            break;
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

void RemoteService::handleFile(const std::vector<uint8_t>& in) {
    const uint8_t op = in[0];

    // Reply framing: [op][status][path\0][extra...].
    auto reply = [&](uint8_t status, const std::string& path,
                     const uint8_t* extra, size_t extraLen) {
        std::vector<uint8_t> p;
        p.push_back(op);
        p.push_back(status);
        p.insert(p.end(), path.begin(), path.end());
        p.push_back(0);
        if (extra && extraLen) p.insert(p.end(), extra, extra + extraLen);
        link_->send(plp::Channel::File, p.data(), p.size());
    };

    if (op == FileOp::Write) {
        // [op][pathLen:2 LE][path][data...]
        if (in.size() < 3) return;
        uint16_t plen = in[1] | (in[2] << 8);
        if (in.size() < (size_t)3 + plen) return;
        std::string path((const char*)in.data() + 3, plen);
        const uint8_t* data = in.data() + 3 + plen;
        size_t dlen = in.size() - 3 - plen;
        reply(fs_->write(path, data, dlen) ? 0 : 2, path, nullptr, 0);
        return;
    }

    // All other ops: [op][path...]
    std::string path((const char*)in.data() + 1, in.size() - 1);
    switch (op) {
        case FileOp::List: {
            std::vector<FsEntry> entries;
            bool ok = fs_->list(path, entries);
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
            reply(ok ? 0 : 1, path, body.data(), body.size());
            break;
        }
        case FileOp::Read: {
            std::vector<uint8_t> data;
            bool ok = fs_->read(path, data);
            reply(ok ? 0 : 1, path, data.data(), data.size());
            break;
        }
        case FileOp::Mkdir:
            reply(fs_->mkdir(path) ? 0 : 2, path, nullptr, 0);
            break;
        case FileOp::Remove:
            reply(fs_->remove(path) ? 0 : 2, path, nullptr, 0);
            break;
        default:
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
