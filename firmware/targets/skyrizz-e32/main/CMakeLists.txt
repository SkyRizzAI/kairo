idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
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
