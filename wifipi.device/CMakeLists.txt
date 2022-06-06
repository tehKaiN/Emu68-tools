cmake_minimum_required(VERSION 3.14.0)
project(WiFiPi VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-Os -m68040 -fomit-frame-pointer -mno-bitfield)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(wifipi.device
    src/start.c
    src/device.c
    src/init.c
    src/mbox.c
    src/sdio.c
    src/firmware.c
    src/end.c
)
target_link_libraries(wifipi.device devicetree amiga)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/wifipi.device DESTINATION ./Emu68-WiFi/)
