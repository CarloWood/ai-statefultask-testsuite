# FetchContent was added to 3.14.
cmake_minimum_required( VERSION 3.14...3.16 )

#===============================================================================
# Lets use Hunter (third party library (build) caching).

include( FetchContent )

set( HUNTER_PACKAGES farmhash Boost )
set( HUNTER_Boost_COMPONENTS system )
set( HUNTER_GIT_REPOSITORY "http://github.com/CarloWood/hunter.git" )
set( HUNTER_GIT_TAG "6ba4e9dc71963fb480cbf6ce928cc73143a43944" )        # Hunter
set( HUNTER_FILEPATH_CONFIG "cwm4/cmake/hunter-config.cmake" )

# Fetch the hunter gate repository.
# Note that if a parent project already did a FetchContent_Declare with the same
# name then this line will be ignored. Since we require special support only add-
# ed to the CarloWood repository we therefore use a special name (CWHunterGate).
#
# For other repositories not forked from http://github.com/CarloWood/gate please
# use a different name.
set( hg_name "CWHunterGate" CACHE INTERNAL
     "The FetchContent_Declare name for HunterGate." )

FetchContent_Declare( ${hg_name}
    GIT_REPOSITORY "http://github.com/CarloWood/gate.git"
    GIT_TAG "2576f8f69e82c5af93049d63808ed5927ebc6e05"          # Gate
            # Or just use "master" to get the latest version.
)
FetchContent_MakeAvailable( ${hg_name} )

#[[
string( TOLOWER ${hg_name} lc_name )
string( TOUPPER ${hg_name} uc_name )

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
    SHA1 "6ba4e9dc71963fb480cbf6ce928cc73143a43944"             # Hunter
    FILEPATH "cwm4/cmake/hunter-config.cmake"
        # This is relative to the CMAKE_CURRENT_SOURCE_DIR
        # (the directory containing this CMakeLists.txt).
)

#]]

#===============================================================================

project( ai-statefultask-testsuite
         LANGUAGES C CXX
         DESCRIPTION "Testsuite for git submodule ai-statefultask." )

#hunter_add_package( farmhash )

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
