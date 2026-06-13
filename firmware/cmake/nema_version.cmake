# nema_version.cmake — reads firmware/VERSION + git hash, generates nema/version.h.
# Included by firmware/core/CMakeLists.txt (works in both ESP-IDF and host/WASM builds).

get_filename_component(_FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ── Parse VERSION file ────────────────────────────────────────────────────────
# The file may carry a Release Please annotation, e.g. "0.1.0 # x-release-please-version".
# Extract the leading MAJOR.MINOR.PATCH with a regex so the comment is ignored.
file(READ "${_FIRMWARE_DIR}/VERSION" _ver_raw)
if(_ver_raw MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(NEMA_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(NEMA_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(NEMA_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
    message(FATAL_ERROR "firmware/VERSION malformed (expected MAJOR.MINOR.PATCH): ${_ver_raw}")
endif()
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
