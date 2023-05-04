cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project(ComputePathTracer 
        DESCRIPTION "A simple compute shader based path tracer made using C++20/DX12"
        LANGUAGES CXX)
            
add_subdirectory(external)
add_subdirectory(src)
