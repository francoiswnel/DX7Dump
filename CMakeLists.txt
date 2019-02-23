cmake_minimum_required(VERSION 3.12)
project(DX7Dump)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(dx7dump main.cpp)