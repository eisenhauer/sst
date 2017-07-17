add_executable(writer writer.c dummy.c)
add_executable(reader reader.c dummy.c)

target_include_directories(writer PRIVATE ..)
target_include_directories(reader PRIVATE ..)

target_link_libraries(writer cp)
target_link_libraries(reader cp)
