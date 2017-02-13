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
define(CW_COMPILE_FLAGS, [-std=c++11 -fmax-errors=2 -W -Wall -Woverloaded-virtual -Wundef -Wpointer-arith -Wwrite-strings -Winline])
# CW_THREADS can be [no] (single-threaded), [yes] (multi-threaded) or [both] (single and multi-threaded applications).
define(CW_THREADS, [yes])

# Include cwm4 header.
m4_include([cwm4/configure_ac_top.m4])

# Comment this out if the project is using doxygen to generate documentation.
CW_DOXYGEN

# Output files.
AC_CONFIG_FILES([src/Makefile])

# Include cwm4 footer.
m4_include([cwm4/configure_ac_bottom.m4])
