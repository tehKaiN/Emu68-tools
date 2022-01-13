cmake_minimum_required(VERSION 3.14.0)
project(Emu68\ VC4 VERSION 0.2.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-Os -m68040 -fomit-frame-pointer -mno-bitfield)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(emu68-vc4.card
    src/main.c
    src/mbox.c
    src/end.c
)
target_link_libraries(emu68-vc4.card devicetree)
