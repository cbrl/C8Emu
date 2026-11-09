cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    c8emu
    VERSION 0.1.0
    DESCRIPTION "A CHIP-8 emulator"
    HOMEPAGE_URL "https://github.com/cbrl/C8Emu"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

# The emulator core. It has no third-party dependencies, so tests can link it
# without the media layer.
add_library(
    c8emu_lib OBJECT
    source/assembler/assembler.cpp
    source/assembler/forms.cpp
    source/assembler/lexer.cpp
    source/assembler/source_map.cpp
    source/chip8/chip8.cpp
    source/chip8/quirks.cpp
    source/chip8/isa/isa.cpp
    source/audio/tone_generator.cpp
    source/debugger/debugger.cpp
    source/frontend/rom_files.cpp
    source/instruction/instruction.cpp
    source/instruction/opcodes.cpp
    source/util/log.cpp
)

target_include_directories(
    c8emu_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(c8emu_lib PUBLIC cxx_std_23)

# ---- Declare executable ----

find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imgui-color-text-edit CONFIG REQUIRED)
find_package(imgui-memory-editor CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

add_executable(
    c8emu_exe
    source/main.cpp
    source/app/app.cpp
    source/app/commands.cpp
    source/app/session.cpp
    source/app/source_document.cpp
    source/platform/beeper.cpp
    source/platform/display_texture.cpp
    source/platform/file_dialogs.cpp
    source/platform/platform.cpp
    source/ui/chip8_language.cpp
    source/ui/command_ui.cpp
    source/ui/dock_layout.cpp
    source/ui/main_menu.cpp
    source/ui/status_bar.cpp
    source/ui/ui.cpp
    source/ui/panels/breakpoints_panel.cpp
    source/ui/panels/display_panel.cpp
    source/ui/panels/editor_panel.cpp
    source/ui/panels/log_panel.cpp
    source/ui/panels/memory_panel.cpp
    source/ui/panels/program_panel.cpp
    source/ui/panels/registers_panel.cpp
    source/ui/panels/settings_panel.cpp
    source/ui/panels/stack_panel.cpp
    source/ui/panels/syntax_reference_panel.cpp
)
add_executable(c8emu::exe ALIAS c8emu_exe)

set_property(TARGET c8emu_exe PROPERTY OUTPUT_NAME c8emu)

target_compile_features(c8emu_exe PRIVATE cxx_std_23)

target_link_libraries(
    c8emu_exe PRIVATE
    c8emu_lib
    glad::glad
    imgui::imgui
    imgui-color-text-edit::imgui-color-text-edit
    imgui-memory-editor::imgui-memory-editor
    nfd::nfd
    SDL3::SDL3
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT c8emu_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of c8emu"
  )
endif()

include(cmake/dev-mode.cmake)
