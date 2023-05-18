add_executable(${PROJECT_NAME} main.cpp engine.cpp graphics_device.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC  ../include)
target_link_libraries(${PROJECT_NAME} PUBLIC external d3d12.lib d3dcompiler.lib dxcompiler.lib dxgi.lib)

target_precompile_headers(${PROJECT_NAME} PUBLIC
    <iostream>
    <span>
    <source_location>
    <exception>
    <cstdint>
    <stdexcept>
    <format>
    <chrono>
    <string_view>
    <memory>
    <vector>
    <array>

    <Windows.h>
    <wrl.h>
    <d3d12.h>
    <dxgi1_6.h>
)