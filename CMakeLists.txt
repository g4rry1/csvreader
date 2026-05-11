set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.10)
project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


file(GLOB SOURCES "src/*.c")


add_executable(csvreader ${SOURCES})


target_include_directories(csvreader PRIVATE include)