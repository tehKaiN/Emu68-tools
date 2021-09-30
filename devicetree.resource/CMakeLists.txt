cmake_minimum_required(VERSION 3.14.0)
project(devicetree VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e_device_start)
add_compile_options(-Os -m68020 -fomit-frame-pointer)

add_executable(devicetree.resource
    main.c
)

add_custom_command(
    TARGET devicetree.resource POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/devicetree.resource" "${CMAKE_CURRENT_BINARY_DIR}/devicetree.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_compile_definitions(devicetree.resource PRIVATE VERSION_STRING="${VERSTRING}")