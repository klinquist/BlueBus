cmake_minimum_required(VERSION 3.20)
project(bluebus_cd53_simulator C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../firmware/application")
set(SIMULATOR_SOURCES
    src/bluebus_sim.c
    src/host_config.c
    src/host_runtime.c
    src/host_timer.c
    ${FIRMWARE_DIR}/lib/event.c
    ${FIRMWARE_DIR}/lib/ibus.c
    ${FIRMWARE_DIR}/ui/cd53.c
    ${FIRMWARE_DIR}/ui/menu/menu_singleline.c
)

add_library(bluebus_sim STATIC ${SIMULATOR_SOURCES})
target_include_directories(bluebus_sim PUBLIC
    include
    host_include
    ${FIRMWARE_DIR}
)
target_compile_options(bluebus_sim PRIVATE -ffunction-sections -fdata-sections)

enable_testing()
add_executable(bluebus_sim_tests tests/test_simulator.c)
target_link_libraries(bluebus_sim_tests PRIVATE bluebus_sim m)
if(APPLE)
    target_link_options(bluebus_sim_tests PRIVATE "-Wl,-dead_strip")
else()
    target_link_options(bluebus_sim_tests PRIVATE "-Wl,--gc-sections")
endif()
add_test(NAME bluebus_simulator COMMAND bluebus_sim_tests)

find_package(Curses REQUIRED)
add_executable(bluebus_cd53_sim src/tui.c)
target_include_directories(bluebus_cd53_sim PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(bluebus_cd53_sim PRIVATE bluebus_sim m ${CURSES_LIBRARIES})
set_target_properties(bluebus_cd53_sim PROPERTIES OUTPUT_NAME "bluebus-cd53-sim")
install(TARGETS bluebus_cd53_sim RUNTIME DESTINATION bin)
