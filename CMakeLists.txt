cmake_minimum_required(VERSION 3.10)
project(rpi-doom-touch C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Architecture-specific optimizations for Raspberry Pi 4B (Cortex-A72)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv7l|armv8l|aarch64")
    message(STATUS "Configuring ARM Cortex-A72 compiler optimizations...")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32-bit armhf (Raspberry Pi OS Legacy)
        add_compile_options(-O3 -march=armv8-a+crc -mtune=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard)
    else()
        # 64-bit arm64
        add_compile_options(-O3 -march=armv8-a+crc -mtune=cortex-a72)
    endif()
else()
    add_compile_options(-O3 -Wall -Wextra)
endif()

find_package(Threads REQUIRED)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
endif()

if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

include_directories(src ${SDL2_INCLUDE_DIRS})

# Core hardware and engine static library
add_library(doom_core STATIC
    src/hardware/config_manager.c
    src/hardware/touch_overlay.c
    src/hardware/matrix_keypad.c
    src/engine/doom_bridge.c
)
target_link_libraries(doom_core Threads::Threads m)
if(SDL2_FOUND)
    target_link_libraries(doom_core ${SDL2_LIBRARIES})
endif()

# Main executable: doom-rpi-touch
add_executable(doom-rpi-touch src/main.c)
target_link_libraries(doom-rpi-touch doom_core Threads::Threads m)
if(SDL2_FOUND)
    target_link_libraries(doom-rpi-touch ${SDL2_LIBRARIES})
endif()

# Standalone Keypad Tool
add_executable(test_gpio_matrix tools/test_gpio_matrix.c)
target_link_libraries(test_gpio_matrix doom_core Threads::Threads m)

# Unit Test Suite
enable_testing()

add_executable(test_config_manager tests/test_config_manager.c src/hardware/config_manager.c)
add_test(NAME test_config_manager COMMAND test_config_manager)

add_executable(test_touch_math tests/test_touch_math.c src/hardware/touch_overlay.c src/hardware/config_manager.c)
target_link_libraries(test_touch_math m)
add_test(NAME test_touch_math COMMAND test_touch_math)

add_executable(test_touch_render tests/test_touch_render.c src/hardware/touch_overlay.c src/hardware/config_manager.c)
target_link_libraries(test_touch_render m)
add_test(NAME test_touch_render COMMAND test_touch_render)

add_executable(test_matrix_keypad tests/test_matrix_keypad.c src/hardware/matrix_keypad.c src/hardware/config_manager.c)
target_link_libraries(test_matrix_keypad Threads::Threads)
add_test(NAME test_matrix_keypad COMMAND test_matrix_keypad)

add_executable(test_event_bridge tests/test_event_bridge.c src/engine/doom_bridge.c src/hardware/touch_overlay.c src/hardware/matrix_keypad.c src/hardware/config_manager.c)
target_link_libraries(test_event_bridge Threads::Threads m)
add_test(NAME test_event_bridge COMMAND test_event_bridge)
