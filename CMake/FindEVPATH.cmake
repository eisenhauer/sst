# - Find EVPATH library, routines for scientific, parallel IO
#   https://www.olcf.ornl.gov/center-projects/evpath/
#
# Use this module by invoking find_package with the form:
#   find_package(EVPATH
#     [version] [EXACT]     # Minimum or EXACT version, e.g. 1.6.0
#     [REQUIRED]            # Fail with an error if EVPATH or a required
#                           #   component is not found
#     [QUIET]               # ...
#     [COMPONENTS <...>]    # Compiled in components: 
#   )
#
# Module that finds the includes and libraries for a working EVPATH install.
# This module invokes the `evpath_config` script that should be installed with
# the other EVPATH tools.
#
# To provide a hint to the module where to find the EVPATH installation,
# set the EVPATH_ROOT environment variable.
#
# If this variable is not set, make sure that at least the according `bin/`
# directory of EVPATH is in your PATH environment variable.
#
# Set the following CMake variables BEFORE calling find_packages to
# influence this module:
#   EVPATH_USE_STATIC_LIBS - Set to ON to force the use of static
#                           libraries.  Default: OFF
#
# This module will define the following variables:
#   EVPATH_INCLUDE_DIRS    - Include directories for the EVPATH headers.
#   EVPATH_LIBRARIES       - EVPATH libraries.
#   EVPATH_FOUND           - TRUE if FindEVPATH found a working install
#   EVPATH_VERSION         - Version in format Major.Minor.Patch
#
# Not used for now:
#   EVPATH_DEFINITIONS     - Compiler definitions you should add with
#                           add_definitions(${EVPATH_DEFINITIONS})
#
# Example to find EVPATH (default)
# find_package(EVPATH)
# if(EVPATH_FOUND)
#   include_directories(${EVPATH_INCLUDE_DIRS})
#   add_executable(foo foo.c)
#   target_link_libraries(foo ${EVPATH_LIBRARIES})
# endif()

# Example to find EVPATH using component
# find_package(EVPATH COMPONENTS fortran)
# if(EVPATH_FOUND)
#   include_directories(${EVPATH_INCLUDE_DIRS})
#   add_executable(foo foo.c)
#   target_link_libraries(foo ${EVPATH_LIBRARIES})
# endif()
###############################################################################
#Copyright (c) 2014, Axel Huebl and Felix Schmitt from http://picongpu.hzdr.de
#All rights reserved.

#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:

#1. Redistributions of source code must retain the above copyright notice, this
#list of conditions and the following disclaimer.

#2. Redistributions in binary form must reproduce the above copyright notice,
#this list of conditions and the following disclaimer in the documentation
#and/or other materials provided with the distribution.

#3. Neither the name of the copyright holder nor the names of its contributors
#may be used to endorse or promote products derived from this software without
#specific prior written permission.

#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################


###############################################################################
# Required cmake version
###############################################################################

cmake_minimum_required(VERSION 2.8.5)


###############################################################################
# EVPATH
###############################################################################

# we start by assuming we found EVPATH and falsify it if some
# dependencies are missing (or if we did not find EVPATH at all)
set(EVPATH_FOUND TRUE)


# find `evpath_config` program #################################################
#   check the EVPATH_ROOT hint and the normal PATH
find_file(EVPATH_CONFIG
    NAME evpath_config
    PATHS $ENV{EVPATH_ROOT}/bin $ENV{EVPATH_DIR}/bin $ENV{INSTALL_PREFIX}/bin $ENV{PATH})

if(EVPATH_CONFIG)
    message(STATUS "Found 'evpath_config': ${EVPATH_CONFIG}")
else(EVPATH_CONFIG)
    set(EVPATH_FOUND FALSE)
    message(STATUS "Can NOT find 'evpath_config' - set EVPATH_ROOT, EVPATH_DIR or INSTALL_PREFIX, or check your PATH")
endif(EVPATH_CONFIG)

# check `evpath_config` program ################################################
if(EVPATH_FOUND)
    execute_process(COMMAND ${EVPATH_CONFIG} -l
                    OUTPUT_VARIABLE EVPATH_LINKFLAGS
                    RESULT_VARIABLE EVPATH_CONFIG_RETURN
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT EVPATH_CONFIG_RETURN EQUAL 0)
        set(EVPATH_FOUND FALSE)
        message(STATUS "Can NOT execute 'evpath_config' - check file permissions")
    endif()

    # find EVPATH_ROOT_DIR
    execute_process(COMMAND ${EVPATH_CONFIG} -d
                    OUTPUT_VARIABLE EVPATH_ROOT_DIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT IS_DIRECTORY "${EVPATH_ROOT_DIR}")
        set(EVPATH_FOUND FALSE)
        message(STATUS "The directory provided by 'evpath_config -d' does not exist: ${EVPATH_ROOT_DIR}")
    endif()
    execute_process(COMMAND ${EVPATH_CONFIG} -c
                    OUTPUT_VARIABLE EVPATH_INCLUDE_PATH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REGEX REPLACE "-I" "" EVPATH_INCLUDE_DIRS ${EVPATH_INCLUDE_PATH})
    STRING(REGEX REPLACE " " ";" EVPATH_INCLUDE_DIRS ${EVPATH_INCLUDE_DIRS})
endif(EVPATH_FOUND)

# option: use only static libs ################################################
if(EVPATH_USE_STATIC_LIBS)
    # carefully: we have to restore the original path in the end
    set(_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()


# we found something in EVPATH_ROOT_DIR and evpath_config works #################
if(EVPATH_FOUND)

    # check for compiled in dependencies, remove ";" in EVPATH_LINKFLAGS (from cmake build)
    string(REGEX REPLACE ";" " " EVPATH_LINKFLAGS "${EVPATH_LINKFLAGS}")
    message(STATUS "EVPATH linker flags (unparsed): ${EVPATH_LINKFLAGS}")

    # find all library paths -L
    #   note: this can cause trouble if some libs are specified twice from
    #         different sources (quite unlikely)
    #         http://www.cmake.org/pipermail/cmake/2008-November/025128.html
    set(EVPATH_LIBRARY_DIRS "")
    string(REGEX MATCHALL " -L([A-Za-z_0-9/\\.-]+)" _EVPATH_LIBDIRS " ${EVPATH_LINKFLAGS}")
    foreach(_LIBDIR ${_EVPATH_LIBDIRS})
        string(REPLACE " -L" "" _LIBDIR ${_LIBDIR})
        list(APPEND EVPATH_LIBRARY_DIRS ${_LIBDIR})
    endforeach()
    # we could append ${CMAKE_PREFIX_PATH} now but that is not really necessary

    message(STATUS "EVPATH DIRS to look for libs: ${EVPATH_LIBRARY_DIRS}")

    # parse all -lname libraries and find an absolute path for them
    string(REGEX MATCHALL " -l([A-Za-z_0-9\\.\\-\\+]+)" _EVPATH_LIBS " ${EVPATH_LINKFLAGS}")
    foreach(_LIB ${_EVPATH_LIBS})
        string(REPLACE " -l" "" _LIB ${_LIB})

        # find static lib: absolute path in -L then default
        find_library(_LIB_DIR NAMES ${_LIB} PATHS ${EVPATH_LIBRARY_DIRS} CMAKE_FIND_ROOT_PATH_BOTH)

        # found?
        if(_LIB_DIR)
            message(STATUS "Found ${_LIB} in ${_LIB_DIR}")
            list(APPEND EVPATH_LIBRARIES "${_LIB_DIR}")
        else(_LIB_DIR)
            set(EVPATH_FOUND FALSE)
            message(STATUS "EVPATH: Could NOT find library '${_LIB}'")
        endif(_LIB_DIR)

        # clean cached var
        unset(_LIB_DIR CACHE)
        unset(_LIB_DIR)
    endforeach()

    #add libraries which are already using cmake format
    string(REGEX REPLACE " " ";" EVPATH_LIBS_SUB "${EVPATH_LINKFLAGS}")
    foreach(foo ${EVPATH_LIBS_SUB})
    if (EXISTS ${foo})
        message("Appending: ${foo}")
        list(APPEND EVPATH_LIBRARIES "${foo}")
    endif()
    endforeach(foo)

    # add the version string
    execute_process(COMMAND ${EVPATH_CONFIG} -v
                    OUTPUT_VARIABLE EVPATH_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    
endif(EVPATH_FOUND)

# unset checked variables if not found
if(NOT EVPATH_FOUND)
    unset(EVPATH_INCLUDE_DIRS)
    unset(EVPATH_LIBRARIES)
endif(NOT EVPATH_FOUND)


# restore CMAKE_FIND_LIBRARY_SUFFIXES if manipulated by this module ###########
if(EVPATH_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()


###############################################################################
# FindPackage Options
###############################################################################

# handles the REQUIRED, QUIET and version-related arguments for find_package
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EVPATH
    REQUIRED_VARS EVPATH_LIBRARIES EVPATH_INCLUDE_DIRS
    VERSION_VAR EVPATH_VERSION
)
