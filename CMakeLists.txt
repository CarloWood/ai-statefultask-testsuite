# FetchContent was added to 3.14.
cmake_minimum_required( VERSION 3.14...3.16 )

#===============================================================================
# Lets use Hunter (third party library (build) caching).

include( FetchContent )

# Fetch the hunter gate repository.
# Note that if a parent project already did a FetchContent_Declare with the same
# name then this line will be ignored. Since we require special support only add-
# ed to the CarloWood repository we therefore use a special name (CWHunterGate).
#
# For other repositories not forked from http://github.com/CarloWood/gate please
# use a different name.
set( hg_name "CWHunterGate" CACHE INTERNAL
     "The FetchContent_Declare name for HunterGate." )

string( TOLOWER ${hg_name} lc_name )
string( TOUPPER ${hg_name} uc_name )
FetchContent_Declare( ${hg_name}
    GIT_REPOSITORY "http://github.com/CarloWood/gate.git"
    GIT_TAG "cb4c66a5831f06fb23aaf294974f19150ccdc096"
            # Or just use "master" to get the latest version.
)
# Allows a parent project to specify its own FetchContent_Declare for ${hg_name}.
if ( NOT ${lc_name}_POPULATED )
  # While working on gate; set FETCHCONTENT_SOURCE_DIR_HUNTERGATE to the local
  # gate-submodule. This allows to safely edit gate without having to push it
  # to github and update the above hash.
  #set( FETCHCONTENT_SOURCE_DIR_${uc_name}
  #     "${CMAKE_CURRENT_SOURCE_DIR}/gate" CACHE PATH "Path to gate submodule" )
  FetchContent_Populate( ${hg_name} )
  # Source the fetched content.
  include( ${${lc_name}_SOURCE_DIR}/cmake/HunterGate.cmake )
endif ()
# Our HunterGate allows to use GIT_REPOSITORY and specify a git repository.
# In that case, the specified SHA1 is the git commit hash that must be checked
# out (detached HEAD).
HunterGate(
    GIT_REPOSITORY "https://github.com/CarloWood/hunter.git"
    SHA1 "5a40fad19c759ee1e99cc43ce22aea1b1f96358e"
    FILEPATH "cwm4/cmake/hunter-config.cmake"
        # This is relative to the CMAKE_CURRENT_SOURCE_DIR
        # (the directory containing this CMakeLists.txt).
)
#===============================================================================

project( ai-statefultask-testsuite
         LANGUAGES C CXX
         DESCRIPTION "Testsuite for git submodule ai-statefultask." )

hunter_add_package( farmhash )
hunter_add_package( matrixssl )

# This project uses aicxx modules.
include( cwm4/cmake/AICxxProject )

# Exit if someone tries to contaminate the source directory with an in-source build.
if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
  message( FATAL_ERROR "Please do out-of-source builds."
                       "Cleanup: \"rm -rf CMake CMakeCache.txt CMakeFiles/\"" )
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
