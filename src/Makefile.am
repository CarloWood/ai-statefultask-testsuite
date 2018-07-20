AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = helloworld fibonacci fiboquick filelock runthread function objectqueue threadpool cv_wait \
	       timer_test timer_thread signal_test benchmark mutex_benchmark test_frequency_counter AITimer_test \
	       evio_test evio_buffer_test evio_threads evio_filedescriptor evio_socket evio_ofstream_data_test

helloworld_SOURCES = helloworld.cxx
helloworld_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
helloworld_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fibonacci_SOURCES = fibonacci.cxx
fibonacci_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
fibonacci_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fiboquick_SOURCES = fiboquick.cxx
fiboquick_CXXFLAGS =
fiboquick_LDADD =

filelock_SOURCES = filelock.cxx
filelock_CXXFLAGS = @LIBCWD_R_FLAGS@ -O3
filelock_LDADD = ../cwds/libcwds_r.la

runthread_SOURCES = runthread.cxx
runthread_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
runthread_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

function_SOURCES = function.cxx
function_CXXFLAGS = @LIBCWD_R_FLAGS@
function_LDADD = ../cwds/libcwds_r.la

objectqueue_SOURCES = objectqueue.cxx
objectqueue_CXXFLAGS = @LIBCWD_R_FLAGS@
objectqueue_LDADD = ../cwds/libcwds_r.la

threadpool_SOURCES = threadpool.cxx
threadpool_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
threadpool_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

cv_wait_SOURCES = cv_wait.cxx
cv_wait_CXXFLAGS = -O3
cv_wait_LDFLAGS = -pthread

timer_test_SOURCES = timer_test.cxx
timer_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
timer_test_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la
timer_test_LDFLAGS = -pthread

timer_thread_SOURCES = timer_thread.cxx
timer_thread_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
timer_thread_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

signal_test_SOURCES = signal_test.cxx
signal_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread -O3
signal_test_LDADD = ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

benchmark_SOURCES = benchmark.cxx
benchmark_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread -O3
benchmark_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

mutex_benchmark_SOURCES = mutex_benchmark.cxx
mutex_benchmark_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread -O3
mutex_benchmark_LDADD = ../cwds/libcwds_r.la

test_frequency_counter_SOURCES = test_frequency_counter.cxx
test_frequency_counter_CXXFLAGS = @LIBCWD_R_FLAGS@
test_frequency_counter_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

AITimer_test_SOURCES = AITimer_test.cxx
AITimer_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
AITimer_test_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

evio_test_SOURCES = evio_test.cxx
evio_test_CXXFLAGS =
evio_test_LDADD = ../evio/libev-4.24/libev.la

evio_threads_SOURCES = evio_threads.cxx
evio_threads_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
evio_threads_LDADD = ../evio/libevio.la ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../evio/libev-4.24/libev.la ../cwds/libcwds_r.la

evio_filedescriptor_SOURCES = evio_filedescriptor.cxx
evio_filedescriptor_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
evio_filedescriptor_LDADD = ../evio/libevio.la ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../evio/libev-4.24/libev.la ../cwds/libcwds_r.la

evio_socket_SOURCES = evio_socket.cxx
evio_socket_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
evio_socket_LDADD = ../evio/libevio.la ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../evio/libev-4.24/libev.la ../cwds/libcwds_r.la

evio_ofstream_data_test_SOURCES = evio_ofstream_data_test.cxx
evio_ofstream_data_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
evio_ofstream_data_test_LDADD = ../evio/libevio.la ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../evio/libev-4.24/libev.la ../cwds/libcwds_r.la

evio_buffer_test_SOURCES = evio_buffer_test.cxx
evio_buffer_test_CXXFLAGS = @LIBCWD_R_FLAGS@ -pthread
evio_buffer_test_LDADD = ../evio/libevio.la ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../evio/libev-4.24/libev.la ../cwds/libcwds_r.la

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
