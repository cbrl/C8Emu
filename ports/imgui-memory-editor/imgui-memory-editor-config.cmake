include(CMakeFindDependencyMacro)
find_dependency(imgui CONFIG)

if(NOT TARGET imgui-memory-editor::imgui-memory-editor)
  get_filename_component(
      _imgui_memory_editor_prefix "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE
  )
  add_library(imgui-memory-editor::imgui-memory-editor INTERFACE IMPORTED)
  set_target_properties(
      imgui-memory-editor::imgui-memory-editor PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${_imgui_memory_editor_prefix}/include"
      INTERFACE_LINK_LIBRARIES imgui::imgui
  )
  unset(_imgui_memory_editor_prefix)
endif()
