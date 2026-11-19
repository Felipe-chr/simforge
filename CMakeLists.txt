cmake_minimum_required(VERSION 3.24)
project(SimForge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc /MP)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
endif()

option(SIMFORGE_BUILD_TESTS "Compila o alvo de testes" ON)
option(SIMFORGE_BUILD_CLI "Compila o CLI" ON)
option(SIMFORGE_BUILD_BENCH "Compila os benchmarks (fora do ctest)" ON)
option(SIMFORGE_STRICT_DETERMINISM "Rejeita relógio de parede no núcleo" ON)

add_subdirectory(src)

if(SIMFORGE_BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(SIMFORGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
