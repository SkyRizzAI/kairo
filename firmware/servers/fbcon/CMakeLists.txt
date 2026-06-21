# FbCon — Linux-style text-console display server (Plan 80, own server module).
#
# A swappable IDisplayServer backend with only core dependencies (Canvas +
# nema::display fonts + IDisplayServer + CliService) — no widget toolkit, no
# theme. Sibling to the aether lib; a target links whichever servers it wants.
#
# Dual build: ESP-IDF component on device, plain static lib on host.

set(FBCON_SRCS src/fbcon_server.cpp)

if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${FBCON_SRCS}
        INCLUDE_DIRS "include"
        REQUIRES core
    )
else()
    add_library(fbcon STATIC ${FBCON_SRCS})
    target_include_directories(fbcon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(fbcon PUBLIC cxx_std_17)
    target_link_libraries(fbcon PUBLIC nema_core)
endif()
