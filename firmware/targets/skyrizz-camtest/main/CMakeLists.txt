idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        espressif__arduino-esp32
        driver
        esp_driver_cam
        esp_mm
)
