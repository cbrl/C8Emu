cmake_minimum_required(VERSION 3.16)

project(imgui-color-text-edit LANGUAGES CXX)

find_package(imgui CONFIG REQUIRED)

add_library(imgui-color-text-edit TextEditor.cpp)
add_library(imgui-color-text-edit::imgui-color-text-edit ALIAS imgui-color-text-edit)

target_compile_features(imgui-color-text-edit PUBLIC cxx_std_17)
target_link_libraries(imgui-color-text-edit PUBLIC imgui::imgui)
target_include_directories(
    imgui-color-text-edit
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)

install(
    TARGETS imgui-color-text-edit
    EXPORT imgui-color-text-edit-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES TextEditor.h DESTINATION include/imgui-color-text-edit)
install(
    EXPORT imgui-color-text-edit-targets
    NAMESPACE imgui-color-text-edit::
    DESTINATION share/imgui-color-text-edit
)

file(
    WRITE "${CMAKE_CURRENT_BINARY_DIR}/imgui-color-text-edit-config.cmake"
    [[
include(CMakeFindDependencyMacro)
find_dependency(imgui CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/imgui-color-text-edit-targets.cmake")
]]
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/imgui-color-text-edit-config.cmake"
    DESTINATION share/imgui-color-text-edit
)
