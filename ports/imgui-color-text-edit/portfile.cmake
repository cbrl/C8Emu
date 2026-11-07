vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO goossens/ImGuiColorTextEdit
    REF "v${VERSION}"
    SHA512 99245f5102e37578b08051efd5f0eee9b44fa6fc3ca2b935e740f5c0574874197f3543b0a687a1091e124dbc5d2b4a384167fe4b0ffb3eb754c1c174fb2d3e44
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
