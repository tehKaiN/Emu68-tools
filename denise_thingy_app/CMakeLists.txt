cmake_minimum_required(VERSION 3.14.0)
project(denise_thingy_app VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-m68040 -m68881 -noixemul)
add_compile_options(-O3 -m68040 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(DeniseThingy
    src/main.c
)

target_link_libraries(DeniseThingy common amiga i2c)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/DeniseThingy DESTINATION ./DeniseThingy/)
install(FILES DeniseThingy.info DESTINATION ./DeniseThingy/)
