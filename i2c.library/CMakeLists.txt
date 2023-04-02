cmake_minimum_required(VERSION 3.14.0)
project(i2c VERSION 40.0.0) # Compat version with i2clib40 from aminet
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-Os -m68040 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_library(i2c INTERFACE)
target_include_directories(i2c INTERFACE include)

add_executable(i2c.library
    src/main.c
    src/gpio.c
    src/end.c

    src/lib_functions/alloc_i2c.c
    src/lib_functions/bring_back_i2c.c
    src/lib_functions/free_i2c.c
    src/lib_functions/get_i2c_opponent.c
    src/lib_functions/i2c_err_text.c
    src/lib_functions/init_i2c.c
    src/lib_functions/receive_i2c.c
    src/lib_functions/send_i2c.c
    src/lib_functions/set_i2c_delay.c
    src/lib_functions/shutdown_i2c.c
)
target_include_directories(i2c.library PRIVATE src)
target_link_libraries(i2c.library i2c devicetree amiga common)
target_compile_definitions(i2c.library PUBLIC
    LIBRARY_NAME="i2c.library"
    LIBRARY_VERSION=${PROJECT_VERSION_MAJOR}
    LIBRARY_REVISION=${PROJECT_VERSION_MINOR}
    LIBRARY_PRIORITY=0)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/i2c.library DESTINATION ./I2C/)
