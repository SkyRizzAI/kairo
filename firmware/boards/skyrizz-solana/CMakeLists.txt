idf_component_register(
    SRCS
        "src/skyrizz_solana.cpp"
        "src/tca9534.cpp"
        "src/lcd_driver.cpp"
        "src/solana_key_map.cpp"
        "src/touch_panel.cpp"
        "src/se050_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        core
        esp32
        espressif__arduino-esp32
        driver
        se05x          # NXP nano-package (SE050 real driver, Plan 96)
)
