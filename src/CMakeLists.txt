cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo)

# some platform builds will have defined CMAKE_SYSTEM_NAME
# from the command line as they require setup to occur before project()
# 
if (CMAKE_SYSTEM_NAME)
  include(cmake/platform_init.cmake)
endif()

project(letterboxed VERSION 0.1.0 LANGUAGES CXX)

if (NOT BNG_PLATFORM)
  include(cmake/platform_init.cmake)
endif()

include(cmake/platform_config.cmake)
#include(cmake/commands.cmake)

set(CMAKE_INCLUDE "${PROJECT_SOURCE_DIR}/cmake")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR} 
)

add_subdirectory(core)
add_subdirectory(word_db)
add_subdirectory(letterboxed)
