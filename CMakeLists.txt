# FetchContent was added to 3.14.
cmake_minimum_required(VERSION 3.14...3.16)

#===============================================================================
# Lets use Hunter (third party library (build) caching).

include(FetchContent)

set(HUNTER_PACKAGES farmhash Boost)
set(HUNTER_Boost_COMPONENTS filesystem iostreams program_options system)
set(HUNTER_GIT_REPOSITORY "http://github.com/CarloWood/hunter.git")
set(HUNTER_GIT_TAG "6ba4e9dc71963fb480cbf6ce928cc73143a43944")          # Hunter
set(HUNTER_FILEPATH_CONFIG "${CMAKE_CURRENT_LIST_DIR}/cwm4/cmake/hunter-config.cmake")
set(HUNTER_CONFIGURATION_TYPES Release)

# Fetch the hunter gate repository.
# Note that if a parent project already did a FetchContent_Declare with the same
# name then this line will be ignored. Since we require special support only add-
# ed to the CarloWood repository we therefore use a special name (CWHunterGate).
#
# For other repositories not forked from http://github.com/CarloWood/gate please
# use a different name.
set(hg_name "CWHunterGate" CACHE INTERNAL
    "The FetchContent_Declare name for the Hunter gate.")

FetchContent_Declare(${hg_name}
  GIT_REPOSITORY "http://github.com/CarloWood/gate.git"
  GIT_TAG "2fea25c8557f3c86502830d4b7d2c7445a043c7b"                    # Gate
          # Or just use "master" to get the latest version.
)
FetchContent_MakeAvailable(${hg_name})

#===============================================================================

project(ai-statefultask-testsuite
  LANGUAGES C CXX
  DESCRIPTION "Testsuite for git submodule ai-statefultask."
)

# This project uses aicxx modules.
include(cwm4/cmake/AICxxProject)

# Exit if someone tries to contaminate the source directory with an in-source build.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Please do out-of-source builds."
                      "Cleanup: \"rm -rf CMake CMakeCache.txt CMakeFiles/\"")
endif ()

include(AICxxSubmodules)

# Do not build the googlemock subproject.
set(BUILD_GMOCK OFF CACHE BOOL "")
# Disable installation of googletest.
set(INSTALL_GTEST OFF CACHE BOOL "")

add_subdirectory(helloworld-task)
add_subdirectory(filelock-task)
add_subdirectory(resolver-task)
add_subdirectory(socket-task)

add_subdirectory(src)
