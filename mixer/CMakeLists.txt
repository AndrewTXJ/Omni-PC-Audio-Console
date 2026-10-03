add_executable(omni-mixer main.cpp)
target_link_libraries(omni-mixer PRIVATE omni_alsa omni_warnings omni_strict_fp Threads::Threads)
