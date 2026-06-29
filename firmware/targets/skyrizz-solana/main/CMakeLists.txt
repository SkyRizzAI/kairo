idf_component_register(
    # wifi_raw_override.c MUST live in main: ESP-IDF links main with
    # --whole-archive, so our strong ieee80211_raw_frame_sanity_check() overrides
    # the (weakened) libnet80211.a symbol. See ADR 0011.
    SRCS "main.cpp" "wifi_raw_override.c"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES
        core
        aether
        esp32
        skyrizz-solana
        espressif__arduino-esp32
)

# Package firmware/assets/ into a LittleFS image flashed to the "spiffs" partition
# (populates /anims/*.panim for the live wallpaper + Dolphin app). Same as skyrizz-e32.
littlefs_create_partition_image(spiffs
    "${CMAKE_CURRENT_LIST_DIR}/../../../assets"
    FLASH_IN_PROJECT
)
