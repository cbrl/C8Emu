include(cmake/folders.cmake)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()

add_custom_target(
    run-exe
    COMMAND c8emu_exe
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
add_dependencies(run-exe c8emu_exe)

option(ENABLE_COVERAGE "Enable coverage support separate from CTest's" OFF)
if(ENABLE_COVERAGE)
  include(cmake/coverage.cmake)
endif()

add_folders(Project)
