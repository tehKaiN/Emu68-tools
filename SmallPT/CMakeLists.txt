cmake_minimum_required(VERSION 3.14.0)
project(SmallPT VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-m68040 -m68881 -noixemul)
add_compile_options(-O3 -m68040 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(SmallPT
    src/main.c
    src/support.c
    src/renderer.c
    src/smallpt.cc
)

set_target_properties(SmallPT PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(SmallPT amiga m)
