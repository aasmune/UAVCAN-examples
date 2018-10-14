cmake_minimum_required(VERSION 3.1)
project(example)

set(CMAKE_C_COMPILER gcc)


# Libcanard
include_directories(src)
include_directories(lib/libcaCCnard)
include_directories(lib/libcanard/drivers/socketcan)

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -Wall -Wextra -Werror -m32 -pedantic")

# C warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wtype-limits")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-conversion -Wcast-align -Wmissing-declarations")

# Example application
exec_program("git"
             ${CMAKE_CURRENT_SOURCE_DIR}
             ARGS "rev-parse --short=8 HEAD"
             OUTPUT_VARIABLE GIT_HASH)

add_executable(example
               src/example.c
               lib/libcanard/canard.c
               lib/libcanard/drivers/socketcan/socketcan.c)

target_compile_definitions(example
                           PUBLIC GIT_HASH=0x${GIT_HASH})