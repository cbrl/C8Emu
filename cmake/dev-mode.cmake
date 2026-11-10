include(cmake/folders.cmake)

add_custom_target(
    run-exe
    COMMAND c8emu_exe
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
add_dependencies(run-exe c8emu_exe)

add_folders(Project)
