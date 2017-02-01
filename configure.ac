# Define package version here.
define(CW_VERSION_MAJOR, 1)
define(CW_VERSION_MINOR, 0)
define(CW_VERSION_REVISION, 0)
define(CW_PACKAGE_NAME, [ai-statefultask-testsuite])
define(CW_BUGREPORT, [carlo@alinoe.com])

# Include cwm4 header.
m4_include([cwm4/configure_ac_top.m4])

# Add --enable-debug (DEBUG, DOXYGEN_DEBUG), --enable-libcwd (CWDEBUG, DOXYGEN_CWDEBUG), --enable-optimize and --enable-profile options.
# Update USE_LIBCWD, LIBCWD(_R)_FLAGS, LIBCWD(_R)_LIBS and CXXFLAGS accordingly.
# Second parameter can be [no] (single-threaded), [yes] (multi-threaded) or [both] (single and multi-threaded applications).
CW_OPG_CXXFLAGS([-fmax-errors=2 -W -Wall -Woverloaded-virtual -Wundef -Wpointer-arith -Wwrite-strings -Winline], [yes])

# Comment this out if the project is using doxygen to generate documentation.
CW_DOXYGEN

# Output files.
AC_CONFIG_FILES([src/Makefile])

# Include cwm4 footer.
m4_include([cwm4/configure_ac_bottom.m4])
