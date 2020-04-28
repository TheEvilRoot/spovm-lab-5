cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fsanitize=address -Werror")

project(0005)

add_subdirectory(evilasyncio)
add_subdirectory(handler)

