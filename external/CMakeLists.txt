include(FetchContent)

FetchContent_Declare(
    SDL2
	GIT_REPOSITORY https://github.com/libsdl-org/SDL
	GIT_TAG release-2.26.0 
	GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL2)

add_library(external INTERFACE)
target_link_libraries(external INTERFACE SDL2::SDL2main SDL2::SDL2test SDL2::SDL2-static)
target_include_directories(external INTERFACE ${sdl2_SOURCE_DIR}/include)