add_library(kairo_board_sim STATIC
    src/simulator_board.cpp
)

target_include_directories(kairo_board_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kairo_board_sim PUBLIC kairo_core)
