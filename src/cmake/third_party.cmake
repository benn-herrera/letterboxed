if(NOT THIRD_PARTY_DIR)
  message(FATAL_ERROR "set THIRD_PARTY_DIR before including")
endif()

function(add_3p_header_lib NAME REPO VERSION INTERFACE_DIR GLOB)
  set(LIB_DIR "${THIRD_PARTY_DIR}/${NAME}")
  if((NOT INTERFACE_DIR) OR (INTERFACE_DIR STREQUAL "."))
    set(INTERFACE_DIR "${LIB_DIR}")
  else()
    set(INTERFACE_DIR "${LIB_DIR}/${INTERFACE_DIR}")
  endif()

  set(VERSION_FILE "${LIB_DIR}/.bng_3p_version")
  if (EXISTS "${VERSION_FILE}")
    file(READ "${VERSION_FILE}" CUR_VERSION)
  endif()
  if (NOT CUR_VERSION STREQUAL VERSION)
    file(REMOVE_RECURSE "${LIB_DIR}")
    Git("${THIRD_PARTY_DIR}" clone "${REPO}" -c advice.detachedHead=false --depth 1 --branch "${VERSION}" "${NAME}")
    file(WRITE "${VERSION_FILE}" "${VERSION}")
  endif()

  file(${GLOB} H_HEADERS RELATIVE "${THIRD_PARTY_DIR}" "${INTERFACE_DIR}/*.h")
  file(${GLOB} HPP_HEADERS RELATIVE "${THIRD_PARTY_DIR}" "${INTERFACE_DIR}/*.hpp")

  file(${GLOB} C_SOURCES RELATIVE "${THIRD_PARTY_DIR}" "${INTERFACE_DIR}/*.c")
  file(${GLOB} CPP_SOURCES RELATIVE "${THIRD_PARTY_DIR}" "${INTERFACE_DIR}/*.cpp")

  add_library(${NAME} INTERFACE
    ${H_HEADERS} ${HPP_HEADERS} ${C_SOURCES}  ${CPP_SOURCES}
  )
  set_target_properties(${NAME}
      PROPERTIES
      FOLDER "third_party/"
  )
endfunction()
