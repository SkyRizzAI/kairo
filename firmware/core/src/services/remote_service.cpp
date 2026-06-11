#include "kairo/services/remote_service.h"
#include "kairo/services/cli_service.h"
#include "kairo/services/input_service.h"
#include "kairo/hal/filesystem.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include "kairo/ui/key.h"
#include <cstring>
#include <string>
#include <vector>

namespace kairo {

void RemoteService::init(LinkService& link, InputService& input) {
    link_ = &link;
    input_ = &input;
    logSink_.link = &link;
    link_->onFrame(&RemoteService::onFrameThunk, this);
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
        link_->send(klp::Channel::Event, p.data(), p.size());
    });
}

void RemoteService::onFrameThunk(void* user, const klp::Frame& f) {
    static_cast<RemoteService*>(user)->dispatch(f);
}

void RemoteService::dispatch(const klp::Frame& f) {
    switch ((klp::Channel)f.channel) {
        case klp::Channel::Input:
            if (!f.payload.empty() && input_)
                input_->post((Key)f.payload[0]);
            break;
        case klp::Channel::System:
            if (f.payload.empty()) break;
            if (f.payload[0] == SysOp::GetInfo) {
                // Reply [opcode][board-profile json] so the host can multiplex
                // future SYSTEM replies by the first byte.
                std::vector<uint8_t> p;
                p.reserve(1 + info_.size());
                p.push_back(SysOp::GetInfo);
                p.insert(p.end(), info_.begin(), info_.end());
                link_->send(klp::Channel::System, p.data(), p.size());
            } else if (f.payload[0] >= SysOp::Restart && powerFn_) {
                powerFn_(powerUser_, f.payload[0]);
            }
            break;
        case klp::Channel::Cli: {            // host→device terminal: [command line]
            if (!cli_ || f.payload.empty()) break;
            std::string line((const char*)f.payload.data(), f.payload.size());
            cli_->execute(line, [this](const std::string& s) {
                link_->send(klp::Channel::Cli, (const uint8_t*)s.data(), s.size());
            });
            const uint8_t eot = 0x04;        // mark end-of-output → host re-prompts
            link_->send(klp::Channel::Cli, &eot, 1);
            break;
        }
        case klp::Channel::File:             // host→device filesystem request
            if (fs_ && !f.payload.empty()) handleFile(f.payload);
            break;
        case klp::Channel::Ext: {            // host→device sim control
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
        link_->send(klp::Channel::File, p.data(), p.size());
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
    link->send(klp::Channel::Log, p.data(), p.size());
}

} // namespace kairo
