// Plan 83 — AppContext concrete methods.
#include "nema/app/app_context.h"
#include "nema/runtime.h"

namespace nema {

AppStorage AppContext::storage() {
    return AppStorage(bundleId(), runtime().fs(), runtime().config(), false);
}

AppStorage AppContext::criticalStorage() {
    return AppStorage(bundleId(), runtime().fs(), runtime().config(), true);
}

} // namespace nema
