AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = helloworld fibonacci fiboquick filelock runthread function objectqueue threadpool cv_wait timer_test

helloworld_SOURCES = \
	helloworld.cxx

fibonacci_SOURCES = \
	fibonacci.cxx

fiboquick_SOURCES = \
	fiboquick.cxx

filelock_SOURCES = \
	filelock.cxx

runthread_SOURCES = \
	runthread.cxx

function_SOURCES = \
	function.cxx

objectqueue_SOURCES = \
	objectqueue.cxx

threadpool_SOURCES = \
	threadpool.cxx

cv_wait_SOURCES = \
	cv_wait.cxx

timer_test_SOURCES = \
	timer_test.cxx

helloworld_CXXFLAGS = @LIBCWD_R_FLAGS@
helloworld_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fibonacci_CXXFLAGS = @LIBCWD_R_FLAGS@
fibonacci_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fiboquick_CXXFLAGS =
fiboquick_LDADD =

filelock_CXXFLAGS = @LIBCWD_R_FLAGS@
filelock_LDADD = ../cwds/libcwds_r.la ../microbench/libmicrobench.la

runthread_CXXFLAGS = @LIBCWD_R_FLAGS@
runthread_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

function_CXXFLAGS = @LIBCWD_R_FLAGS@
function_LDADD = ../cwds/libcwds_r.la ../microbench/libmicrobench.la

objectqueue_CXXFLAGS = @LIBCWD_R_FLAGS@
objectqueue_LDADD = ../cwds/libcwds_r.la

threadpool_CXXFLAGS = @LIBCWD_R_FLAGS@
threadpool_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

cv_wait_CXXFLAGS = -O3
cv_wait_LDFLAGS = -pthread

timer_test_CXXFLAGS = @LIBCWD_R_FLAGS@
timer_test_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la
timer_test_LDFLAGS = -pthread

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
