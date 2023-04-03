cmake_minimum_required(VERSION 3.14.0)
project(SetClockI2C VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -m68040 -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-O3 -m68040 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(SetClockI2C
    src/main.c
)

set_target_properties(SetClockI2C PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(SetClockI2C i2c common)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/SetClockI2C DESTINATION ./I2C/)
