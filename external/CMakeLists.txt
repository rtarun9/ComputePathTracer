include(FetchContent)

FetchContent_Declare(
    SDL2
	GIT_REPOSITORY https://github.com/libsdl-org/SDL
	GIT_TAG release-2.26.0 
	GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG 54c1ac3e38a5b6df4c2c016cd5859666f3472e4d
    GIT_PROGRESS TRUE
)
    
FetchContent_MakeAvailable(imgui)

add_library(libimgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)

target_include_directories(libimgui PUBLIC 
${imgui_SOURCE_DIR} 
${imgui_SOURCE_DIR}/backends
${sdl2_SOURCE_DIR}/include
)

target_link_libraries(libimgui PUBLIC SDL2::SDL2main SDL2::SDL2-static)

add_library(external INTERFACE)
target_link_libraries(external INTERFACE SDL2::SDL2main SDL2::SDL2test SDL2::SDL2-static libimgui)
target_include_directories(external INTERFACE ${sdl2_SOURCE_DIR}/include)