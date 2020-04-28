cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/build)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(CMAKE_CXX_FLAGS "-fsanitize=address -Werror")

project(handler)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} handler.cpp)

target_link_libraries(${PROJECT_NAME} Threads::Threads)
target_link_libraries(${PROJECT_NAME} rt)
target_link_libraries(${PROJECT_NAME} dl)
