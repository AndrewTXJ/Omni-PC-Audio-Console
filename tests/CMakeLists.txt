# Every test is its own executable with its own main(), so a crash in one cannot
# hide the others and ctest reports them separately.
set(OMNI_TESTS
  test_qs01_unity
  test_qs05_gain_pan
  test_qs04_isolation
  test_qs07_ramps
  test_dsp_units
)

foreach(t IN LISTS OMNI_TESTS)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE omni_engine omni_warnings omni_strict_fp)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${t} COMMAND ${t})
  set_tests_properties(${t} PROPERTIES
    ENVIRONMENT "OMNI_TEST_TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
