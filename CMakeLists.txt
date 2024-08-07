# FetchContent was added to 3.14.
cmake_minimum_required(VERSION 3.14...3.17)

# Compile all subdirectories with the same standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(ai-statefultask-testsuite
  LANGUAGES C CXX
  DESCRIPTION "Testsuite for git submodule ai-statefultask."
)

# Begin of gitache configuration.
set(GITACHE_PACKAGES libcwd_r wolfssl magic_enum)

# Is libfarmhash installed somewhere in a default path?
find_library(
  FARMHASH farmhash
  DOC "Whether or not libfarmhash is installed locally."
)
# If not, use gitache to get it.
if(NOT FARMHASH)
  list(APPEND GITACHE_PACKAGES farmhash)
endif()

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
  GIT_TAG        "master"       # Normally use either "stable" or a fixed SHA1 to freeze the version here!
                                # Using "master" might be *unstable*.
)
FetchContent_MakeAvailable(gitache)
# End of gitache configuration.

# This project uses aicxx modules.
include(cwm4/cmake/AICxxProject NO_POLICY_SCOPE)

# Exit if someone tries to contaminate the source directory with an in-source build.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Please do out-of-source builds."
                      "Cleanup: \"rm -rf CMake CMakeCache.txt CMakeFiles/\"")
endif ()

include(AICxxSubmodules)

# Extra clean up before removing directories.
add_custom_target(maintainer-clean-extra
  COMMAND rm -f CMakeDoxygenDefaults.cmake CMakeDoxyfile.in
)

# Do not build the googlemock subproject.
set(BUILD_GMOCK OFF CACHE BOOL "")
# Disable installation of googletest.
set(INSTALL_GTEST OFF CACHE BOOL "")

add_subdirectory(helloworld-task)
add_subdirectory(filelock-task)
add_subdirectory(resolver-task)
add_subdirectory(socket-task)

add_subdirectory(src)
