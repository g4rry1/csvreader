cmake_minimum_required(VERSION 3.10)
project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES "src/*.c")

add_executable(csvreader ${SOURCES})

target_include_directories(csvreader PRIVATE include)

target_compile_options(csvreader PRIVATE -Wall -Wextra)

option(ASAN "Enable AddressSanitizer" OFF)
if(ASAN)
  target_compile_options(csvreader PRIVATE -fsanitize=address -g)
  target_link_options(csvreader PRIVATE -fsanitize=address)
endif()
