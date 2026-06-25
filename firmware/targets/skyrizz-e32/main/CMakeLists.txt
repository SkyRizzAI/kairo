idf_component_register(
    # wifi_raw_override.c MUST live in main: ESP-IDF links main with
    # --whole-archive, so our strong ieee80211_raw_frame_sanity_check() is
    # force-included and overrides the (weakened) libnet80211.a symbol. In a
    # normal component it sits in an archive and is never pulled, because the
    # weak blob symbol already satisfies the reference. See ADR 0011.
    SRCS "main.cpp" "wifi_raw_override.c"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE          # force-link wifi_raw_override.c — its only symbol is
                           # already weakly defined by libnet80211.a, so without
                           # this it is never pulled and the override is dead.
    REQUIRES
        core
        aether
        esp32
        skyrizz-e32
        espressif__arduino-esp32
)

# Package firmware/assets/ into a LittleFS image flashed to the "spiffs" partition.
# This populates /anims/*.panim so the live wallpaper and Dolphin app have data.
# The partition is formatted as LittleFS (via joltwallet__littlefs) but labelled
# "spiffs" in partitions.csv — that's just a flash layout label, not the driver.
littlefs_create_partition_image(spiffs
    "${CMAKE_CURRENT_LIST_DIR}/../../../assets"
    FLASH_IN_PROJECT
)
