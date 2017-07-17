cmake_minimum_required(VERSION 3.6.2)
cmake_policy(VERSION 3.6.2)

project(SST)

# Fail immediately if not using an out-of-source build
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not supported.  Please create a build directory "
    "separate from the source directory")
endif()

#------------------------------------------------------------------------------#
# Some boilerplate to setup nice output directories
#------------------------------------------------------------------------------#

include(GNUInstallDirs)

list(INSERT CMAKE_MODULE_PATH 0 "${SST_SOURCE_DIR}/cmake")
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${SST_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${SST_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
endif()
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${SST_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
endif()

# Let windows builds auto-export dll symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

# Default to a debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

include(CMakeDependentOption)

# Setup shared library / -fPIC stuff
get_property(SHARED_LIBS_SUPPORTED GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
cmake_dependent_option(SST_BUILD_SHARED_LIBS
  "Whether or not to build shared libraries" ON
  "SHARED_LIBS_SUPPORTED" OFF)
cmake_dependent_option(SST_ENABLE_PIC
  "Build with Position Independent Code" ON
  "SHARED_LIBS_SUPPORTED" OFF)
set(BUILD_SHARED_LIBS ${SST_BUILD_SHARED_LIBS})


if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall ")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall ")
endif()

include(CTest)

add_subdirectory(dp)
add_subdirectory(cp)
add_subdirectory(framework)

#------------------------------------------------------------------------------#
# Configuration summary
#------------------------------------------------------------------------------#
message("")
message("SST build configuration:")
message("  SST Version: ${SST_VERSION}")
message("  C++ Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_CXX_COMPILER_WRAPPER}")
message("    ${CMAKE_CXX_COMPILER}")
message("")
message("  Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message("  Features:")
if(BUILD_SHARED_LIBS)
  message("    Library Type: shared")
else()
  message("    Library Type: static")
endif()
message("    Build Type:   ${CMAKE_BUILD_TYPE}")
message("    Testing: ${BUILD_TESTING}")
message("")

