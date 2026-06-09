#include "kairo/services/remote_service.h"
#include "kairo/services/input_service.h"
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
                link_->send(klp::Channel::System,
                            (const uint8_t*)info_, std::strlen(info_));
            } else if (f.payload[0] >= SysOp::Restart && powerFn_) {
                powerFn_(powerUser_, f.payload[0]);
            }
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
