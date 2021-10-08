cmake_minimum_required(VERSION 3.14.0)
project(brcm-sdhc VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68020 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(brcm-sdhc.device
    src/main.c
    src/init.c
    src/beginio.c
    src/abortio.c
)

add_custom_command(
    TARGET brcm-sdhc.device POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/brcm-sdhc.device" "${CMAKE_CURRENT_BINARY_DIR}/brcm-sdhc.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(brcm-sdhc.device PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
target_link_libraries(brcm-sdhc.device devicetree)
