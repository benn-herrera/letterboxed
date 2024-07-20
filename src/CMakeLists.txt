cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo)

# some platform builds will have defined CMAKE_SYSTEM_NAME
# from the command line as they require setup to occur before project()
# 
if (CMAKE_SYSTEM_NAME)
  include(cmake/platform_init.cmake)
endif()

project(bng VERSION 0.1.0 LANGUAGES CXX C)

if (NOT BNG_PLATFORM)
  include(cmake/platform_init.cmake)
endif()

include(cmake/platform_config.cmake)
#include(cmake/commands.cmake)

set(CMAKE_INCLUDE "${PROJECT_SOURCE_DIR}/cmake")

set(TARGET letterboxed)
include("${CMAKE_INCLUDE}/target_exe.cmake")

add_custom_command(TARGET letterboxed
  COMMAND 
  "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/words_alpha.txt" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
)
