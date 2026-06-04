idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        core
        esp32
        dev-board
        espressif__arduino-esp32
)
