file(GLOB_RECURSE SIMFORGE_TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(simforge_tests ${SIMFORGE_TEST_SOURCES})
target_link_libraries(simforge_tests PRIVATE simforge_core simforge_c)
target_include_directories(simforge_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(simforge_tests PRIVATE
  SIMFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# A DLL é carregada em tempo de execução; copie-a ao lado do executável.
add_custom_command(TARGET simforge_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:simforge_c>"
          "$<TARGET_FILE_DIR:simforge_tests>"
)

add_test(NAME simforge_tests COMMAND simforge_tests)
