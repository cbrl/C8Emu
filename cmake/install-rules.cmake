install(
    TARGETS c8emu_exe
    RUNTIME COMPONENT c8emu_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
