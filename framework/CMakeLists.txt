add_executable(writer writer.c dummy.c)
add_executable(reader reader.c dummy.c)

target_include_directories(writer PRIVATE .. ${MPI_C_INCLUDE_PATH})
target_include_directories(reader PRIVATE .. ${MPI_C_INCLUDE_PATH})

set_property(TARGET writer reader PROPERTY C_STANDARD 99)

target_link_libraries(writer sst)
target_link_libraries(reader sst)
