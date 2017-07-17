set(dp_source_files
  dp.c
  dummy_dp.c
)
add_library(dp ${dp_source_files})

target_include_directories(dp PUBLIC ${FFS_INCLUDE_DIR}
${EVPATH_INCLUDE_DIR} ..)

