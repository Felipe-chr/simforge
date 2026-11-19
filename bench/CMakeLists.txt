add_executable(simforge_bench main.cpp)
target_link_libraries(simforge_bench PRIVATE simforge_core)
target_compile_definitions(simforge_bench PRIVATE
  SIMFORGE_BENCH_CONFIG="${CMAKE_BUILD_TYPE}"
)
