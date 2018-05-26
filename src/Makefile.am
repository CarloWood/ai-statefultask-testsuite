AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = helloworld fibonacci fiboquick filelock runthread function objectqueue threadpool cv_wait timer_test timer_thread signal_test benchmark mutex_benchmark test_frequency_counter

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

timer_thread_SOURCES = \
	timer_thread.cxx

signal_test_SOURCES = \
	signal_test.cxx

benchmark_SOURCES = \
	benchmark.cxx

mutex_benchmark_SOURCES = \
	mutex_benchmark.cxx

test_frequency_counter_SOURCES = \
	test_frequency_counter.cxx

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

timer_thread_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
timer_thread_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

signal_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread -O3
signal_test_LDADD = ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

benchmark_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
benchmark_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

mutex_benchmark_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread -O3
mutex_benchmark_LDADD = ../cwds/libcwds_r.la

test_frequency_counter_CXXFLAGS = @LIBCWD_R_FLAGS@
test_frequency_counter_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
