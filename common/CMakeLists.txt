cmake_minimum_required(VERSION 3.14.0)
project(common VERSION 1.0.0)

add_library(common INTERFACE)
target_include_directories(common INTERFACE inc)
