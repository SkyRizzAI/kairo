// Plan 83 — AppContext concrete methods.
#include "nema/app/app_context.h"
#include "nema/runtime.h"

namespace nema {

void AppContext::warmStorage() {
    if (!storage_)     storage_.emplace(bundleId(), runtime().fs(), runtime().config(), false);
    if (!critStorage_) critStorage_.emplace(bundleId(), runtime().fs(), runtime().config(), true);
}

AppStorage& AppContext::storage() {
    if (!storage_) storage_.emplace(bundleId(), runtime().fs(), runtime().config(), false);
    return *storage_;
}

AppStorage& AppContext::criticalStorage() {
    if (!critStorage_) critStorage_.emplace(bundleId(), runtime().fs(), runtime().config(), true);
    return *critStorage_;
}

} // namespace nema
