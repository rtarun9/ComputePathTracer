add_executable(${PROJECT_NAME} main.cpp "engine.cpp")

target_include_directories(${PROJECT_NAME} PUBLIC  ../include)
target_link_libraries(${PROJECT_NAME} PUBLIC external)

target_precompile_headers(${PROJECT_NAME} PUBLIC
    <iostream>
    <span>
    <source_location>
    <exception>
    <cstdint>
    <stdexcept>
    <format>
    <string_view>
    <vector>
    <array>

    <Windows.h>
)