#pragma once
#include "kairo/service.h"
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <type_traits>

namespace kairo {

class ServiceContainer {
public:
    // Register by concrete type. If T is an IService, it's also tracked for lifecycle.
    template <class T>
    void registerService(T* instance) {
        assert(instance);
        byType_[std::type_index(typeid(T))] = static_cast<void*>(instance);
        if constexpr (std::is_base_of_v<IService, T>) {
            services_.push_back(static_cast<IService*>(instance));
        }
    }

    // Also register an instance under a different interface type (e.g. abstract base).
    template <class I, class T>
    void registerAs(T* instance) {
        static_assert(std::is_base_of_v<I, T>);
        byType_[std::type_index(typeid(I))] = static_cast<void*>(static_cast<I*>(instance));
    }

    template <class T>
    T* resolve() {
        auto it = byType_.find(std::type_index(typeid(T)));
        if (it == byType_.end()) return nullptr;
        return static_cast<T*>(it->second);
    }

    template <class T>
    T& require() {
        T* p = resolve<T>();
        assert(p && "required service not registered");
        return *p;
    }

    const std::vector<IService*>& services() const { return services_; }

private:
    std::unordered_map<std::type_index, void*> byType_;
    std::vector<IService*> services_;   // insertion-order for start/stop sequencing
};

} // namespace kairo
