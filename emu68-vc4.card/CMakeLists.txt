cmake_minimum_required(VERSION 3.14.0)
project(emu68-vc4 VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles)
add_compile_options(-Os -m68020 -fomit-frame-pointer -mno-bitfield)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(emu68-vc4.card
    src/main.c
)
