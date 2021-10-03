cmake_minimum_required(VERSION 3.14.0)
project(devicetree VERSION 1.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68000 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_library(devicetree INTERFACE)

add_executable(devicetree.resource
    src/main.c
    src/init.c
    src/closekey.c
    src/openkey.c
    src/findproperty.c
    src/getpropname.c
    src/getpropvalue.c
    src/getproplen.c
    src/getchild.c
    src/getproperty.c
)

target_include_directories(devicetree.resource PRIVATE include)
target_include_directories(devicetree INTERFACE include)

add_custom_command(
    TARGET devicetree.resource POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/devicetree.resource" "${CMAKE_CURRENT_BINARY_DIR}/devicetree.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(devicetree.resource PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
