# FetchContent was added to 3.14.
cmake_minimum_required(VERSION 3.14...3.17)

project(ai-statefultask-testsuite
  LANGUAGES C CXX
  DESCRIPTION "Testsuite for git submodule ai-statefultask."
)

# Begin of gitache configuration.
set(GITACHE_PACKAGES farmhash libcwd_r)

#include(gitache/gateway.cmake)
# OR
include(FetchContent)

# If a local gitache submodule is present then use that rather than downloading one.
if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/gitache/.git)
  # This will disable the use of the GIT_REPOSITORY/GIT_TAG below, and disable the
  # FetchContent- download and update step. Instead, use the gitache submodule as-is.
  set(FETCHCONTENT_SOURCE_DIR_GITACHE "${CMAKE_CURRENT_LIST_DIR}/gitache" CACHE INTERNAL "" FORCE)
endif ()

FetchContent_Declare(
  gitache
  GIT_REPOSITORY "https://github.com/CarloWood/gitache.git"
  GIT_TAG        "stable"
)
FetchContent_MakeAvailable(gitache)
# End of gitache configuration.

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
