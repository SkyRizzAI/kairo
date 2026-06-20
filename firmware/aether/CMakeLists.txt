# Aether — the 1-bit canvas display server (Plan 80 / ADR 0002).
#
# A swappable display-server module: it OWNS all presentation (UI model, widgets,
# themes, fonts & text, renderer, component system, screens) and depends ONE-WAY on
# nema core (IDisplayDriver, Canvas, IDisplayServer). Core must never depend on this.
#
# Dual build like nema_core: ESP-IDF component on device, plain static lib on host.

set(AETHER_SRCS
    src/aether_info.cpp
    # Phases 1–4 (Plan 80) move presentation sources here from core/src/ui & screens.
)

if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${AETHER_SRCS}
        INCLUDE_DIRS "include"
        REQUIRES core
    )
else()
    add_library(aether STATIC ${AETHER_SRCS})
    target_include_directories(aether PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(aether PUBLIC cxx_std_17)
    target_link_libraries(aether PUBLIC nema_core)
endif()
