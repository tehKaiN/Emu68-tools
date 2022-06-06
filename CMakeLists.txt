cmake_minimum_required(VERSION 3.14.0)
project(Emu68-tools VERSION 0.1.0)

include(cmake/verstring.cmake)
get_verstring(VERSTRING)

add_subdirectory(devicetree.resource)
add_subdirectory(sdcard.device)
add_subdirectory(emu68-vc4.card)
add_subdirectory(68040.library)
add_subdirectory(EmuControl)
add_subdirectory(SmallPT)
add_subdirectory(Buddhabrot)
add_subdirectory(VC4Tweak)
add_subdirectory(wifipi.device)

# Contribs
add_subdirectory(Emu68Contribs/Emu68Info)