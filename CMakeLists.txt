cmake_minimum_required( VERSION 3.12 )

# Lets use Hunter (third party library (build) caching).
include( cwm4/cmake/HunterGate.cmake )

project( ai-statefultask-testsuite
         LANGUAGES C CXX
         DESCRIPTION "Testsuite for git submodule ai-statefultask." )

# This project uses aicxx modules.
include( cwm4/cmake/AICxxProject )

# Exit if someone tries to contaminates the source directory with an in-source build.
if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
  message( FATAL_ERROR "Please do out-of-source builds.\nCleanup: \"rm -rf CMake CMakeCache.txt CMakeFiles/\"" )
endif ()

include( AICxxSubmodules )

# Do not Builds the googlemock subproject.
set( BUILD_GMOCK OFF CACHE BOOL "" )
# Disable installation of googletest.
set( INSTALL_GTEST OFF CACHE BOOL "" )

add_subdirectory( helloworld-task )
add_subdirectory( filelock-task )
add_subdirectory( resolver-task )
add_subdirectory( socket-task )

add_subdirectory( src )
