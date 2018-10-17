cmake_minimum_required(VERSION 3.1)
project(example)

set(CMAKE_C_COMPILER gcc)
find_package(PythonInterp 3 REQUIRED)

# Check out submodules
exec_program("git"
             ${CMAKE_CURRENT_SOURCE_DIR}
             ARGS "submodule update --init --recursive")


# Libcanard
include_directories(src)
include_directories(lib/libcanard)
include_directories(lib/libcanard/drivers/socketcan)

# Compile DSDL
exec_program("${PYTHON_EXECUTABLE}"
             ${CMAKE_CURRENT_SOURCE_DIR}
             ARGS "lib/libcanard/dsdl_compiler/libcanard_dsdlc --header_only --outdir dsdl/ lib/dsdl/uavcan/")

include_directories(dsdl)

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c11   -Wall -Wextra -Werror -m32 -pedantic -g")

# C warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wtype-limits -Wno-error=unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-align -Wmissing-declarations")


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