cmake_minimum_required(VERSION 3.14.0)
project(common VERSION 1.0.0)

add_compile_options(-Os -m68040 -fomit-frame-pointer)
file(GLOB common_sources src/*.c src/*.cpp include/common/*.hpp include/common/*.h)
add_library(common STATIC ${common_sources})
target_include_directories(common PUBLIC include)
