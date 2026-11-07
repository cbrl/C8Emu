# Header-only library
set(VCPKG_BUILD_TYPE release)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ocornut/imgui_club
    REF b083116866e05e0d4bccb2c7f28d94a76ee4965e
    SHA512 64f85b1338714c7496fdc2d486ddce3d9f0da448bac3e432a0e26b20e76de4c81fb78b3cd2d7f34455f8711eab963194b13aa7da53daedb5a06d893de4e337b1
    HEAD_REF main
)

file(
    INSTALL "${SOURCE_PATH}/imgui_memory_editor/imgui_memory_editor.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include"
)
file(
    INSTALL "${CMAKE_CURRENT_LIST_DIR}/imgui-memory-editor-config.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
