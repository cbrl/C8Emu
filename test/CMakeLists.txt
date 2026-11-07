# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(c8emuTests LANGUAGES CXX)

# ---- Dependencies ----

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(c8emu_test source/c8emu_test.cpp)
target_link_libraries(
    c8emu_test PRIVATE
    c8emu_lib
    Catch2::Catch2WithMain
)
target_compile_features(c8emu_test PRIVATE cxx_std_23)

catch_discover_tests(c8emu_test)

# ---- End-of-file commands ----

add_folders(Test)
