add_executable(kairo-sim main.cpp)

target_include_directories(kairo-sim PRIVATE
    ${CMAKE_SOURCE_DIR}/platforms/simulator/include
    ${CMAKE_SOURCE_DIR}/boards/simulator/include
)

target_link_libraries(kairo-sim PRIVATE
    kairo_core
    kairo_platform_sim
    kairo_board_sim
)

target_compile_definitions(kairo-sim PRIVATE
    KAIRO_BUILD_VERSION="0.1.0-mvp"
    KAIRO_FW_VERSION="0.1.0"
)
