idf_component_register(
    SRCS
        "src/skyrizz_e32.cpp"
        "src/xl9535.cpp"
        "src/lcd_driver.cpp"
        "src/e32_key_map.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        core
        esp32
        espressif__arduino-esp32
        driver
)
