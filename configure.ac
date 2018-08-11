# Define package version here.
#
# CW_VERSION_MAJOR               : Increment when the whole library was rewritten; or some VERY MAJOR change was made.
# CW_VERSION_MINOR               : Increment when major changes have occured, like removing interfaces.
# CW_VERSION_REVISION            : Increment every public release; or set to 0 when CW_VERSION_MINOR was incremented.

define(CW_VERSION_MAJOR, 1)
define(CW_VERSION_MINOR, 0)
define(CW_VERSION_REVISION, 0)

# Package name and bug reporting.
define(CW_PACKAGE_NAME, [ai-statefultask-testsuite])
define(CW_BUGREPORT, [carlo@alinoe.com])

# CW_COMPILE_FLAGS may contain warning flags, but also default debug flags (-g*)
# and optimization flags (-O*) that which will be stripped when not required.
define(CW_COMPILE_FLAGS, [-std=c++17 -W -Wall -Woverloaded-virtual -Wundef -Wpointer-arith -Wwrite-strings -Winline])
# CW_THREADS can be [no] (single-threaded), [yes] (multi-threaded) or [both] (single and multi-threaded applications).
define(CW_THREADS, [yes])
# CW_MAX_ERRORS is the maximum number of errors the compiler will show.
define(CW_MAX_ERRORS, [2])

# Include cwm4 header.
m4_include([cwm4/configure_ac_top.m4])

# Remove the default -O2 from CFLAGS when optimization isn't requested.
if test x"${cw_config_optimize}" != x"yes"; then
  CFLAGS=-g
fi

# Comment this out if the project is using doxygen to generate documentation.
CW_DOXYGEN([statefultask])

# Output files.
AC_CONFIG_FILES([src/Makefile] [src/kmod/Makefile])

# Include cwm4 footer.
m4_include([cwm4/configure_ac_bottom.m4])
