macro(forbid_exe_test_targets)
  if (TEST_SOURCES)
    message(FATAL_ERROR "do put tests in exe target ${TARGET}. anything that needs testing belongs in a library.")
  endif()
endmacro()

macro(collect_test_sources)
  file(${SOURCES_GLOB} TEST_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp" )
  file(${SOURCES_GLOB} TEST_HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h" )

  list(REMOVE_ITEM SOURCES ${TEST_SOURCES})
  list(REMOVE_ITEM HEADERS ${TEST_HEADERS})
endmacro()

macro(add_lib_test_targets)
  if(BNG_BUILD_TESTS AND TEST_SOURCES)
    set(RUN_LIB_SUITE_TARGET run_${TARGET}_suite)
    add_custom_target(${RUN_LIB_SUITE_TARGET} DEPENDS ${TARGET})
    set_target_properties(
      ${RUN_LIB_SUITE_TARGET} PROPERTIES 
      EXCLUDE_FROM_ALL TRUE
      EXCLUDE_FROM_DEFAULT_BUILD TRUE
      FOLDER "tests/"
      )
    set(ALL_RUN_TEST_TARGETS ${ALL_RUN_TEST_TARGETS} ${RUN_LIB_SUITE_TARGET} PARENT_SCOPE)  

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dummy.txt"
      COMMAND echo
    )

    foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
      get_filename_component(TEST_TARGET "${TEST_SOURCE}" NAME_WE)
      string(REPLACE "_test_" "" TEST_TARGET "${TEST_TARGET}")
      string(REPLACE "test_" "" TEST_TARGET "${TEST_TARGET}")
      string(REPLACE "test_" "" TEST_TARGET "${TEST_TARGET}")    
      set(TEST_TARGET "test_${TARGET}_${TEST_TARGET}")
      add_executable(${TEST_TARGET} ${TEST_HEADERS} ${TEST_SOURCE})
      set_target_properties(${TEST_TARGET}
          PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/"
          FOLDER "tests/${TARGET}_suite"
      )
      add_dependencies(${RUN_LIB_SUITE_TARGET} ${TEST_TARGET})
      if((CMAKE_GENERATOR MATCHES "Ninja") OR (CMAKE_GENERATOR MATCHES "Make"))
        # single config
        add_custom_command(
          TARGET ${RUN_LIB_SUITE_TARGET}
          DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test.dummy"
          COMMAND "${CMAKE_BINARY_DIR}/tests/${TEST_TARGET}")
      else()
        # multi config
        add_custom_command(
          TARGET ${RUN_LIB_SUITE_TARGET}
          DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test.dummy"
          COMMAND "${CMAKE_BINARY_DIR}/tests/$<IF:$<CONFIG:Debug>,Debug,RelWithDebInfo>/${TEST_TARGET}")      
      endif()
      target_link_libraries(${TEST_TARGET} ${TARGET})
      unset(TEST_TARGET)
    endforeach()
  endif()
endmacro()

macro(add_run_all_tests_target)
  if(ALL_RUN_TEST_TARGETS)
    add_custom_target(run_all_suites DEPENDS ${ALL_RUN_TEST_TARGETS})
    set_target_properties(
      run_all_suites PROPERTIES
      EXCLUDE_FROM_ALL TRUE
      EXCLUDE_FROM_DEFAULT_BUILD TRUE
      FOLDER "tests/" 
      )
    add_custom_target(RUN_ALL_TESTS)
    set_target_properties(RUN_ALL_TESTS PROPERTIES FOLDER "CMakePredefinedTargets")
    add_custom_command(TARGET RUN_ALL_TESTS
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target run_all_suites)
  endif()
endmacro()
