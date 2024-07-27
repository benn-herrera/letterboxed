# target_common.cmake
# defines TARGET from name of current directory
# define HEADERS, SOURCES, AIO_SOURCE

include("${CMAKE_INCLUDE}/test_macros.cmake")

if (NOT TARGET)
  get_filename_component(TARGET "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
endif()

if(AUTO_AIO)
  set(AIO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}_aio.cpp")
  if(EXISTS "${AIO_SOURCE}")
    file(REMOVE "${AIO_SOURCE}")
  endif()
endif()

if(NOT DEFINED SOURCES_GLOB)
  set(SOURCES_GLOB GLOB_RECURSE)
endif()

file(${SOURCES_GLOB} HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(${SOURCES_GLOB} SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# defines TEST_SOURCES, TEST_HEADERS
bng_collect_test_sources()

list(LENGTH SOURCES SOURCE_COUNT)

if(SOURCES AND AUTO_AIO AND (SOURCE_COUNT GREATER 1))
  if(AIO_EXCLUDES)
    list(REMOVE_ITEM SOURCES ${AIO_EXCLUDES})
  endif()
  file(WRITE "${AIO_SOURCE}" "// generated unity build file for ${TARGET}\n")
  foreach(SOURCE IN LISTS SOURCES)
    file(APPEND "${AIO_SOURCE}" "#include \"${SOURCE}\"\n")
  endforeach()
  set_source_files_properties(${SOURCES} PROPERTIES HEADER_FILE_ONLY true)
  if(AIO_EXCLUDES)
    set(SOURCES ${SOURCES} ${AIO_EXCLUDES})
  endif()
else()
  unset(AIO_SOURCE)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(BNG_USE_FOLDERS)
  foreach(HEADER IN LISTS HEADERS)
    get_filename_component(FOLDER "${HEADER}" DIRECTORY)
    if(FOLDER)
      source_group("Header Files/${FOLDER}" "${HEADER}")
    endif()
  endforeach()
  foreach(SOURCE IN LISTS SOURCES)
    get_filename_component(FOLDER "${SOURCE}" DIRECTORY)
    if(FOLDER)
      source_group("Source Files/${FOLDER}" "${SOURCE}")
    endif()
  endforeach()
endif()

macro(bng_add_link_libraries)
  target_link_libraries(${TARGET} ${ARGN})
endmacro()

macro(bng_add_dependencies)
  add_dependencies(${TARGET} ${ARGN})
endmacro()

macro(bng_copy_resources)
  cmake_parse_arguments(COPY_RESOURCES "" "SUBDIR" "FILES;DIRECTORIES" "${ARGN}")
  if(NOT (COPY_RESOURCES_FILES OR COPY_RESOURCES_DIRECTORIES))
    message(FATAL_ERROR "one or more of FILES, DIRECTORIES is required")
  endif()
  
  if(COPY_RESOURCES_SUBDIR)
    add_custom_command(TARGET ${TARGET}
      COMMAND
      "${CMAKE_COMMAND}" -E make_directory
      "$<TARGET_FILE_DIR:${TARGET}>/${COPY_RESOURCES_SUBDIR}/"
    )
  endif()

  if(COPY_RESOURCES_FILES)
    add_custom_command(TARGET ${TARGET}
      COMMAND
      "${CMAKE_COMMAND}" -E copy
      ${COPY_RESOURCES_FILES}
      "$<TARGET_FILE_DIR:${TARGET}>/${COPY_RESOURCES_SUBDIR}/"
    )
  endif()

  if(COPY_RESOURCES_DIRECTORIES)
    add_custom_command(TARGET ${TARGET}
      COMMAND
      "${CMAKE_COMMAND}" -E copy_directory
      ${COPY_RESOURCES_DIRECTORIES}
      "$<TARGET_FILE_DIR:${TARGET}>/${COPY_RESOURCES_SUBDIR}/"
    )
  endif()
endmacro()
