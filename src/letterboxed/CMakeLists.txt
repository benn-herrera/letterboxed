include("${CMAKE_INCLUDE}/target_exe.cmake")

bng_add_link_libraries(word_db)

bng_copy_resources(FILES "${CMAKE_CURRENT_SOURCE_DIR}/words_alpha.txt")
