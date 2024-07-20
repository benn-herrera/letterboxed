find_package(Git REQUIRED)

# for all functions 
# ouptput can be captured by appending OUTPUT_VARIABLE [output_var_name] to the args
# see https://cmake.org/cmake/help/v3.28/command/execute_process.html#execute-process

if(BNG_IS_WINDOWS)
  string(REPLACE "git.exe" "bash.exe" Bash_EXECUTABLE "${GIT_EXECUTABLE}")
  string(REPLACE "git.exe" "curl.exe" Curl_EXECUTABLE "${GIT_EXECUTABLE}")  
  string(REPLACE "/mingw64/" "/" Bash_EXECUTABLE "${Bash_EXECUTABLE}")
  set(Python_EXECUTABLE "${REPO_ROOT_DIR}/.venv/Scripts/python3.exe") 
else()
  set(Bash_EXECUTABLE "/usr/bin/bash")
  set(Curl_EXECUTABLE "/usr/bin/curl")  
  set(Python_EXECUTABLE "${REPO_ROOT_DIR}/.venv/bin/python3") 
endif()

function(Git WORKING_DIR)
  execute_process(COMMAND "${GIT_EXECUTABLE}" ${ARGN}
    WORKING_DIRECTORY "${WORKING_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(Bash WORKING_DIR)
  execute_process(COMMAND "${Bash_EXECUTABLE}" ${ARGN}
    WORKING_DIRECTORY "${WORKING_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()
if(NOT EXISTS "${Bash_EXECUTABLE}")
  message(FATAL_ERROR "${Bash_EXECUTABLE} does not exist.")
endif()

function(CMake WORKING_DIR)
  execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN}
    WORKING_DIRECTORY "${WORKING_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(Curl WORKING_DIR)
  execute_process(COMMAND "${Curl_EXECUTABLE}" ${ARGN}
    WORKING_DIRECTORY "${WORKING_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()
if(NOT EXISTS "${Curl_EXECUTABLE}")
  message(FATAL_ERROR "${Curl_EXECUTABLE} does not exist.")
endif()

function(Python WORKING_DIR)
  execute_process(COMMAND "${Python_EXECUTABLE}" ${ARGN} 
    WORKING_DIRECTORY "${WORKING_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()
if(NOT EXISTS "${Python_EXECUTABLE}")
  message(FATAL_ERROR "${Python_EXECUTABLE} does not exist.")
endif()
