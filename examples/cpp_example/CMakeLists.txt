# MIT License
#
# Copyright (c) 2026 LONGZR007
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.14)
project(eproto_cpp_example)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find source files
set(EPROTO_SOURCES
    ../../src/eproto.c
    ../../src/eproto_crc16.c
    ../../src/eproto_frame_parser.c
    ../../src/eproto_packet_node.c
    ../../src/eproto_ring_buffer.c
)

# Include directories
include_directories(
    .
    ../../inc
)

# Add executable
add_executable(eproto_cpp_example
    main.cpp
    ${EPROTO_SOURCES}
)

# Link threads for std::thread
find_package(Threads REQUIRED)
target_link_libraries(eproto_cpp_example Threads::Threads)

# Compiler warnings
if(MSVC)
    target_compile_options(eproto_cpp_example PRIVATE /W4)
else()
    target_compile_options(eproto_cpp_example PRIVATE -Wall -Wextra -Wpedantic)
endif()
