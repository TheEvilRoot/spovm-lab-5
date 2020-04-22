cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/build)
set(THREADS_PREFER_PTHREAD_FLAG ON)

project(0005)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} Threads::Threads)
target_link_libraries(${PROJECT_NAME} rt)
