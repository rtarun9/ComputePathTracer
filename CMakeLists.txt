cmake_minimum_required(VERSION 3.17)

project(ComputePathTracer)

# Config project options.
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Bin")
    
# Project source files
file(GLOB_RECURSE SRC_FILES
    ${PROJECT_SOURCE_DIR}/**/*.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/Include/
    ${PROJECT_SOURCE_DIR}/Include/Pch.hpp
)

# Not including any of the windows files here because of the #ifdef's used.
target_precompile_headers(${PROJECT_NAME}
    PUBLIC
    <algorithm>
    <chrono>
    <memory>
    <string>
    <vector>
    <array>
    <map>
    <cassert>
    <exception>
    <cmath>
    <random>

    PRIVATE
    ${PROJECT_SOURCE_DIR}/Include/Pch.hpp
)

target_link_libraries(${PROJECT_NAME}) 

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
