file(GLOB_RECURSE SIMFORGE_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/devices/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/boards/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/persistence/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/api/*.cpp"
)
list(FILTER SIMFORGE_CORE_SOURCES EXCLUDE REGEX "/transport_c/")

add_library(simforge_core STATIC ${SIMFORGE_CORE_SOURCES})
target_include_directories(simforge_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_features(simforge_core PUBLIC cxx_std_20)
if(SIMFORGE_STRICT_DETERMINISM)
  target_compile_definitions(simforge_core PUBLIC SIMFORGE_STRICT_DETERMINISM=1)
endif()

# ABI C estável (única fronteira binária do produto)
add_library(simforge_c SHARED "${CMAKE_CURRENT_SOURCE_DIR}/api/transport_c/c_api.cpp")
target_include_directories(simforge_c PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(simforge_c PRIVATE simforge_core)
target_compile_definitions(simforge_c PRIVATE SF_BUILD_SHARED=1)
set_target_properties(simforge_c PROPERTIES
  OUTPUT_NAME simforge
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(SIMFORGE_BUILD_CLI)
  add_executable(simforge_cli "${CMAKE_CURRENT_SOURCE_DIR}/app/cli/main.cpp")
  target_link_libraries(simforge_cli PRIVATE simforge_core)
  set_target_properties(simforge_cli PROPERTIES OUTPUT_NAME simforge-cli)
endif()
