cmake_minimum_required(VERSION 3.22)
project(omni_pc_audio_console LANGUAGES CXX)

# ADR-0003: C++20 with Qt 6/QML. The engine core and the offline renderer need
# neither Qt nor ALSA, so M1 builds and tests anywhere -- which is the point of
# the offline renderer (roadmap 4.1).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(OMNI_WERROR "Treat warnings as errors" ON)

add_library(omni_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(omni_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wnon-virtual-dtor -Wdouble-promotion)
  if(OMNI_WERROR)
    target_compile_options(omni_warnings INTERFACE -Werror)
  endif()
endif()

# The null tests (QS-01, QS-04) compare bit patterns, so the compiler must not
# be allowed to reassociate or contract float arithmetic. -ffast-math would
# silently invalidate every exactness claim in docs/TEST-PLAN.md.
add_library(omni_strict_fp INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(omni_strict_fp INTERFACE
    -fno-fast-math -ffp-contract=off)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(engine)
add_subdirectory(offline)
add_subdirectory(bench)
if(OMNI_HAVE_ALSA)
  add_subdirectory(mixer)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
