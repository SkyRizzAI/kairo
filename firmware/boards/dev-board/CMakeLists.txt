idf_component_register(
    SRCS
        "src/dev_board.cpp"
        "src/tca9534_buttons.cpp"
        "src/dev_board_key_map.cpp"
        "src/eink_display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        core
        esp32
        espressif__arduino-esp32
        GxEPD2
        Adafruit_GFX
)
