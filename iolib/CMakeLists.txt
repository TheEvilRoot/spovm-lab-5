cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(LIBRARY_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/build)
set(THREADS_PREFER_PTHREAD_FLAG ON)

project(io)

find_package(Threads REQUIRED)

add_library(${PROJECT_NAME} SHARED iolib.cpp iolib.h)

target_link_libraries(${PROJECT_NAME} Threads::Threads)
target_link_libraries(${PROJECT_NAME} rt)
