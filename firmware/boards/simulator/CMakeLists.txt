add_library(nema_board_sim STATIC
    src/simulator_board.cpp
)

target_include_directories(nema_board_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nema_board_sim PUBLIC nema_core)
