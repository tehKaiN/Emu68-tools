cmake_minimum_required(VERSION 3.14.0)
project(Emu68-tools VERSION 0.1.0)

include(cmake/verstring.cmake)
get_verstring(VERSTRING)

add_subdirectory(sdcard.device)
add_subdirectory(devicetree.resource)
