include("${CMAKE_INCLUDE}/target_exe.cmake")

add_dependencies(${TARGET} core word_db)

target_link_libraries(${TARGET} word_db)

add_custom_command(TARGET ${TARGET}
  COMMAND
  "${CMAKE_COMMAND}" -E copy 
    "${CMAKE_CURRENT_SOURCE_DIR}/words_alpha.txt" 
    "$<TARGET_FILE_DIR:${TARGET}>/"
)
