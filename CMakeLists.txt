cmake_minimum_required(VERSION 3.1)
project(example)

set(CMAKE_C_COMPILER gcc)
find_package(PythonInterp 3 REQUIRED)


# Libcanard
set(SRC_PATH "src")
set(LIBCANARD_PATH "lib/libcanard")
set(SOCKETCAN_PATH "lib/libcanard/drivers/socketcan")

# Compile DSDL
set(RAW_DSDL_PATH "lib/dsdl/uavcan/")
set(COMPILED_DSDL_PATH "dsdl")
exec_program("${PYTHON_EXECUTABLE}"
             ${CMAKE_CURRENT_SOURCE_DIR}
             ARGS "lib/libcanard/dsdl_compiler/libcanard_dsdlc --outdir ${COMPILED_DSDL_PATH} ${RAW_DSDL_PATH}")

# Serialized DSDL files
file(GLOB_RECURSE DSDL_SRC_PATH "${COMPILED_DSDL_PATH}/*.c")

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c11   -Wall -Wextra -Werror -m32 -pedantic -g")

# C warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wtype-limits -Wno-error=unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-align -Wmissing-declarations")

#Override compile warnings for serialized DSDL files. Need to be done after configuring the rest of the compiler flags.
foreach(dsdl_source_file ${DSDL_SRC_PATH})
    set_source_files_properties(${dsdl_source_file} PROPERTIES COMPILE_FLAGS "-w")
endforeach()

# Include paths
include_directories(${SRC_PATH} ${LIBCANARD_PATH} ${SOCKETCAN_PATH} ${COMPILED_DSDL_PATH})

# Example application
exec_program("git"
             ${CMAKE_CURRENT_SOURCE_DIR}
             ARGS "rev-parse --short=8 HEAD"
             OUTPUT_VARIABLE GIT_HASH)



add_executable(example
               src/example.c
               lib/libcanard/canard.c
               lib/libcanard/drivers/socketcan/socketcan.c
               ${DSDL_SRC_PATH})

target_compile_definitions(example
                           PUBLIC GIT_HASH=0x${GIT_HASH})