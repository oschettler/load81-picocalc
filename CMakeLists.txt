# LOAD81 for PicoCalc
# Port of LOAD81 fantasy console to Clockwork PicoCalc hardware

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug output option - disabled by default (use on-screen REPL instead)
option(DEBUG_OUTPUT "Enable debug output via USB (non-blocking)" OFF)

# 9P server option - enabled by default
option(ENABLE_9P_SERVER "Enable 9P filesystem server over WiFi" ON)

# Board configuration for Pico 2 W (RP2350)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK
include(extern/picocalc-text-start/pico_sdk_import.cmake)

project(load81_picocalc C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Lua 5.2 library
add_library(lua52 STATIC
    extern/load81/lua/src/lapi.c
    extern/load81/lua/src/lauxlib.c
    extern/load81/lua/src/lbaselib.c
    extern/load81/lua/src/lcode.c
    extern/load81/lua/src/ldblib.c
    extern/load81/lua/src/ldebug.c
    extern/load81/lua/src/ldo.c
    extern/load81/lua/src/ldump.c
    extern/load81/lua/src/lfunc.c
    extern/load81/lua/src/lgc.c
    extern/load81/lua/src/linit.c
    extern/load81/lua/src/liolib.c
    extern/load81/lua/src/loadlib.c
    extern/load81/lua/src/loslib.c
    extern/load81/lua/src/llex.c
    extern/load81/lua/src/lmathlib.c
    extern/load81/lua/src/lmem.c
    extern/load81/lua/src/lobject.c
    extern/load81/lua/src/lopcodes.c
    extern/load81/lua/src/lparser.c
    extern/load81/lua/src/lstate.c
    extern/load81/lua/src/lstring.c
    extern/load81/lua/src/lstrlib.c
    extern/load81/lua/src/ltable.c
    extern/load81/lua/src/ltablib.c
    extern/load81/lua/src/ltm.c
    extern/load81/lua/src/lundump.c
    extern/load81/lua/src/lvm.c
    extern/load81/lua/src/lzio.c
)

target_include_directories(lua52 PUBLIC
    extern/load81/lua/src
)

# Disable file I/O to save space (no liolib, loadlib, loslib)
target_compile_definitions(lua52 PRIVATE
    LUA_32BITS
)

# PicoCalc drivers library
add_library(picocalc_drivers STATIC
    extern/picocalc-text-start/drivers/lcd.c
    extern/picocalc-text-start/drivers/keyboard.c
    extern/picocalc-text-start/drivers/southbridge.c
    extern/picocalc-text-start/drivers/sdcard.c
    extern/picocalc-text-start/drivers/fat32.c
    extern/picocalc-text-start/drivers/font-8x10.c
    extern/picocalc-text-start/drivers/clib.c
)

target_include_directories(picocalc_drivers PUBLIC
    extern/picocalc-text-start
    extern/picocalc-text-start/drivers
)

target_link_libraries(picocalc_drivers
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_gpio
)

# 9P Server library (optional)
if(ENABLE_9P_SERVER)
    add_library(picocalc_9p STATIC
        src/picocalc_9p_proto.c
        src/picocalc_9p.c
        src/picocalc_9p_handlers.c
        src/picocalc_9p_fs.c
        src/picocalc_fat32_sync.c
        src/picocalc_mdns.c
        src/picocalc_9p_core1.c
        src/picocalc_debug_log.c
    )
    
    target_include_directories(picocalc_9p PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/extern/picocalc-text-start/drivers
    )
    
    target_link_libraries(picocalc_9p
        picocalc_drivers
        pico_stdlib
        pico_sync
        pico_multicore
        pico_cyw43_arch_lwip_poll
    )
    
    target_compile_definitions(picocalc_9p PRIVATE ENABLE_9P_SERVER)
    
    message(STATUS "9P server enabled")
endif()

# Main executable
add_executable(load81_picocalc
    src/main.c
    src/picocalc_framebuffer.c
    src/picocalc_graphics.c
    src/picocalc_editor.c
    src/picocalc_lua.c
    src/picocalc_menu.c
    src/picocalc_keyboard.c
    src/picocalc_wifi.c
    src/picocalc_nex.c
    src/picocalc_repl.c
    src/picocalc_diag_server.c
    src/picocalc_debug_log.c
)

target_include_directories(load81_picocalc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/extern/load81
    ${CMAKE_CURRENT_LIST_DIR}/extern/picocalc-text-start
    ${CMAKE_CURRENT_LIST_DIR}/extern/picocalc-text-start/drivers
    ${CMAKE_CURRENT_LIST_DIR}/extern/load81/lua/src
)

target_link_libraries(load81_picocalc
    picocalc_drivers
    lua52
    pico_stdlib
    pico_sync
    pico_multicore
    pico_cyw43_arch_lwip_poll
    hardware_spi
    hardware_i2c
    hardware_gpio
)

# Link 9P server if enabled
if(ENABLE_9P_SERVER)
    target_link_libraries(load81_picocalc picocalc_9p)
    target_compile_definitions(load81_picocalc PRIVATE ENABLE_9P_SERVER)
endif()

# Conditionally enable DEBUG_OUTPUT and stdio
if(DEBUG_OUTPUT)
    target_compile_definitions(load81_picocalc PRIVATE DEBUG_OUTPUT)
    # Enable stdio - USB is non-blocking and won't hang without a cable
    # UART would block if no cable is connected, so we use USB instead
    pico_enable_stdio_uart(load81_picocalc 0)
    pico_enable_stdio_usb(load81_picocalc 1)
    message(STATUS "Debug output enabled - outputs to USB when connected")
else()
    # Disable all stdio when debug output is off
    pico_enable_stdio_uart(load81_picocalc 0)
    pico_enable_stdio_usb(load81_picocalc 0)
    message(STATUS "Debug output disabled")
endif()

# Set program metadata
pico_set_program_name(load81_picocalc "LOAD81 for PicoCalc (debug)")
pico_set_program_version(load81_picocalc "1.0")

# Generate UF2 and other outputs
pico_add_extra_outputs(load81_picocalc)

# Turn on warnings
target_compile_options(load81_picocalc PRIVATE -Wall -Wno-unused-variable -Wno-unused-function)
