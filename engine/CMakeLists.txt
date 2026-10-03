add_library(omni_engine
  src/strip.cpp
  src/bus.cpp
  src/graph.cpp
  src/wav.cpp
)
target_include_directories(omni_engine PUBLIC include)
target_link_libraries(omni_engine PRIVATE omni_warnings PUBLIC omni_strict_fp)
