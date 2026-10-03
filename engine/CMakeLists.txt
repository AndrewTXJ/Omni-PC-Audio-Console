add_library(omni_engine
  src/strip.cpp
  src/bus.cpp
  src/graph.cpp
  src/wav.cpp
  src/command.cpp
)
target_include_directories(omni_engine PUBLIC include)
target_link_libraries(omni_engine PRIVATE omni_warnings PUBLIC omni_strict_fp)

# The ALSA backend is optional: the engine core, the offline renderer and every
# null test build without it, which is what lets CI check the quality
# specifications on a machine with no sound card (roadmap 4.1).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(ALSA QUIET IMPORTED_TARGET alsa)
endif()

if(TARGET PkgConfig::ALSA)
  add_library(omni_alsa src/alsa_device.cpp)
  target_include_directories(omni_alsa PUBLIC include)
  target_link_libraries(omni_alsa
    PUBLIC omni_engine PkgConfig::ALSA
    PRIVATE omni_warnings omni_strict_fp)
  set(OMNI_HAVE_ALSA ON CACHE INTERNAL "")
  message(STATUS "ALSA found: building omni_alsa and omni-mixer")
else()
  set(OMNI_HAVE_ALSA OFF CACHE INTERNAL "")
  message(STATUS "ALSA not found: skipping omni_alsa and omni-mixer (engine and omni-render still build)")
endif()
