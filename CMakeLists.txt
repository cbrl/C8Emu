cmake_minimum_required(VERSION 3.8.0)
project(chip8)

# Set options depending on the compiler in use
if (MSVC)
  set(DEBUG_OPTIONS /W4 /Od)
  set(RELEASE_OPTIONS /W4 /O2 /DNDEBUG)
else()
  set(DEBUG_OPTIONS -Wall -Wextra -O0 -g)
  set(RELEASE_OPTIONS -Wall -Wextra -O3 -DNDEBUG)
endif()

# Collect project files
FILE(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Add project executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Configure project properties
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Configure compile options
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")

# Add include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)
