cmake_minimum_required(VERSION 3.8.0)
project(chip8)

# Set options depending on the compiler in use
if (MSVC)
  set(DEBUG_OPTIONS /W3 /Od)
  set(RELEASE_OPTIONS /W3 /O2 /DNDEBUG)
else()
  set(DEBUG_OPTIONS -Wall -Wextra -O0 -g)
  set(RELEASE_OPTIONS -Wall -Wextra -O3 -DNDEBUG)
endif()

find_package(OpenGL REQUIRED)
find_package(glad REQUIRED)
find_package(SDL2 REQUIRED)

# Collect ImGui files
set(IMGUI_SRC
    ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_sdl.cpp
    ${CMAKE_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.cpp
)
# Collect project files
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Add project executable
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${IMGUI_SRC}
)

# Configure project properties
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Configure compile options
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")

# Add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/gl3w
    ${CMAKE_SOURCE_DIR}/src
    ${SDL_INCLUDE}
    ${OPENGL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glad::glad
    SDL2::SDL2
    SDL2::SDL2main
)
