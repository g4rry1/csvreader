cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0077 NEW)
project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library of all sources except main.c so tests can link against it
file(GLOB LIB_SOURCES "src/*.c")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_library(csvlib STATIC ${LIB_SOURCES})
target_include_directories(csvlib PUBLIC include)
target_compile_options(csvlib PRIVATE -Wall -Wextra -Wconversion)

# Main executable
add_executable(csvreader src/main.c)
target_link_libraries(csvreader PRIVATE csvlib)
target_include_directories(csvreader PRIVATE include)
target_compile_options(csvreader PRIVATE -Wall -Wextra -Wconversion)

# ---------------------------------------------------------------------------
# Sanitizers
#   SANITIZE=ON  — AddressSanitizer + LeakSanitizer + UndefinedBehaviorSanitizer
#                  Applied to the library, main executable, and all test binaries.
#                  Enabled by default in Debug builds; pass -DSANITIZE=OFF to disable.
# ---------------------------------------------------------------------------
if(NOT DEFINED SANITIZE)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    set(SANITIZE ON)
  else()
    set(SANITIZE OFF)
  endif()
endif()
option(SANITIZE "Enable ASan + LSan + UBSan" ${SANITIZE})

set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g)
set(SANITIZE_LINK  -fsanitize=address,undefined)

if(SANITIZE)
  message(STATUS "Sanitizers: ASan + LSan + UBSan enabled")
  foreach(_t csvlib csvreader)
    target_compile_options(${_t} PRIVATE ${SANITIZE_FLAGS})
  endforeach()
  target_link_options(csvreader PRIVATE ${SANITIZE_LINK})
endif()

# Tests (requires libcmocka-dev)
enable_testing()

find_library(CMOCKA_LIB cmocka)
if(CMOCKA_LIB)
  add_executable(test_csv tests/test_csv.c)
  target_include_directories(test_csv PRIVATE include)
  target_link_libraries(test_csv PRIVATE csvlib ${CMOCKA_LIB})
  target_compile_options(test_csv PRIVATE -Wall -Wextra -Wconversion)

  add_executable(test_stress tests/test_stress.c)
  target_include_directories(test_stress PRIVATE include)
  target_link_libraries(test_stress PRIVATE csvlib ${CMOCKA_LIB})
  target_compile_options(test_stress PRIVATE -Wall -Wextra -Wconversion)

  if(SANITIZE)
    foreach(_t test_csv test_stress)
      target_compile_options(${_t} PRIVATE ${SANITIZE_FLAGS})
      target_link_options(${_t} PRIVATE ${SANITIZE_LINK})
    endforeach()
  endif()

  add_test(NAME csv_tests    COMMAND test_csv)
  add_test(NAME stress_tests COMMAND test_stress)
else()
  message(WARNING "cmocka not found — unit tests will not be built")
endif()

add_test(
  NAME    binary_tests
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_binary.sh
          $<TARGET_FILE:csvreader>
)
