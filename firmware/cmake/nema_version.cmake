# nema_version.cmake — reads firmware/VERSION + git hash, generates nema/version.h.
# Included by firmware/core/CMakeLists.txt (works in both ESP-IDF and host/WASM builds).

get_filename_component(_FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ── Parse VERSION file ────────────────────────────────────────────────────────
file(READ "${_FIRMWARE_DIR}/VERSION" _ver_raw)
string(STRIP "${_ver_raw}" _ver_raw)
string(REPLACE "." ";" _ver_parts "${_ver_raw}")
list(GET _ver_parts 0 NEMA_VERSION_MAJOR)
list(GET _ver_parts 1 NEMA_VERSION_MINOR)
list(GET _ver_parts 2 NEMA_VERSION_PATCH)
set(NEMA_VERSION "${NEMA_VERSION_MAJOR}.${NEMA_VERSION_MINOR}.${NEMA_VERSION_PATCH}")

# ── Git hash + dirty flag ────────────────────────────────────────────────────
set(NEMA_BUILD_HASH "unknown")
set(NEMA_BUILD_DIRTY 0)
set(_git_tag_rc 1)

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${_FIRMWARE_DIR}"
        OUTPUT_VARIABLE NEMA_BUILD_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY "${_FIRMWARE_DIR}"
        OUTPUT_VARIABLE _git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT "${_git_status}" STREQUAL "")
        set(NEMA_BUILD_DIRTY 1)
    endif()
    # Exact tag match → clean release build (no -dev suffix)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match HEAD
        WORKING_DIRECTORY "${_FIRMWARE_DIR}"
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_tag_rc
    )
endif()

# ── Full version string ──────────────────────────────────────────────────────
if(_git_tag_rc EQUAL 0 AND NOT NEMA_BUILD_DIRTY)
    # HEAD is exactly on a version tag and working tree is clean → release
    set(NEMA_FULL_VERSION "${NEMA_VERSION}")
elseif(NEMA_BUILD_DIRTY)
    set(NEMA_FULL_VERSION "${NEMA_VERSION}-dev+${NEMA_BUILD_HASH}.dirty")
else()
    set(NEMA_FULL_VERSION "${NEMA_VERSION}-dev+${NEMA_BUILD_HASH}")
endif()

# ── Generate header ──────────────────────────────────────────────────────────
configure_file(
    "${_FIRMWARE_DIR}/cmake/version.h.in"
    "${CMAKE_BINARY_DIR}/nema_generated/nema/version.h"
    @ONLY
)

message(STATUS "Nema version: ${NEMA_FULL_VERSION}")
