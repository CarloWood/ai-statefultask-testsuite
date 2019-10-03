AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = helloworld fibonacci fiboquick filelock runthread function objectqueue threadpool cv_wait \
	       timer_test timer_thread signal_test benchmark mutex_benchmark test_frequency_counter AITimer_test \
	       AILookupTask_test AIResolver_test hash_test serv_test proto_test \
	       resolver_getnameinfo socket_task_test FileLock_test AIStatefulTaskMutex_test semaphore_test \
	       spin_wakeup_test

spin_wakeup_test_SOURCES = spin_wakeup_test.cxx
spin_wakeup_test_CXXFLAGS = @LIBCWD_R_FLAGS@
spin_wakeup_test_LDADD = ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

semaphore_test_SOURCES = semaphore_test.cxx
semaphore_test_CXXFLAGS = @LIBCWD_R_FLAGS@
semaphore_test_LDADD = ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

AIStatefulTaskMutex_test_SOURCES = AIStatefulTaskMutex_test.cxx
AIStatefulTaskMutex_test_CXXFLAGS = -O2 @LIBCWD_R_FLAGS@
AIStatefulTaskMutex_test_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../events/libevents.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

FileLock_test_SOURCES = FileLock_test.cxx
FileLock_test_CXXFLAGS = @LIBCWD_R_FLAGS@
FileLock_test_LDADD = ../filelock-task/libfilelocktask.la ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../events/libevents.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

helloworld_SOURCES = helloworld.cxx
helloworld_CXXFLAGS = @LIBCWD_R_FLAGS@
helloworld_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fibonacci_SOURCES = fibonacci.cxx
fibonacci_CXXFLAGS = @LIBCWD_R_FLAGS@
fibonacci_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fiboquick_SOURCES = fiboquick.cxx
fiboquick_CXXFLAGS =
fiboquick_LDADD =

filelock_SOURCES = filelock.cxx
filelock_CXXFLAGS = -O3 @LIBCWD_R_FLAGS@
filelock_LDADD = ../cwds/libcwds_r.la

runthread_SOURCES = runthread.cxx
runthread_CXXFLAGS = @LIBCWD_R_FLAGS@
runthread_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

function_SOURCES = function.cxx
function_CXXFLAGS = @LIBCWD_R_FLAGS@
function_LDADD = ../cwds/libcwds_r.la

objectqueue_SOURCES = objectqueue.cxx
objectqueue_CXXFLAGS = @LIBCWD_R_FLAGS@
objectqueue_LDADD = ../cwds/libcwds_r.la

threadpool_SOURCES = threadpool.cxx
threadpool_CXXFLAGS = @LIBCWD_R_FLAGS@
threadpool_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

cv_wait_SOURCES = cv_wait.cxx
cv_wait_CXXFLAGS = -O3
cv_wait_LDFLAGS = -pthread

timer_test_SOURCES = timer_test.cxx
timer_test_CXXFLAGS = @LIBCWD_R_FLAGS@
timer_test_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la
timer_test_LDFLAGS = -pthread

timer_thread_SOURCES = timer_thread.cxx
timer_thread_CXXFLAGS = @LIBCWD_R_FLAGS@
timer_thread_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

signal_test_SOURCES = signal_test.cxx
signal_test_CXXFLAGS = -O3 @LIBCWD_R_FLAGS@
signal_test_LDADD = ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

benchmark_SOURCES = benchmark.cxx
benchmark_CXXFLAGS = -O3 @LIBCWD_R_FLAGS@
benchmark_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

mutex_benchmark_SOURCES = mutex_benchmark.cxx
mutex_benchmark_CXXFLAGS = -O3 @LIBCWD_R_FLAGS@
mutex_benchmark_LDADD = ../cwds/libcwds_r.la

test_frequency_counter_SOURCES = test_frequency_counter.cxx
test_frequency_counter_CXXFLAGS = @LIBCWD_R_FLAGS@
test_frequency_counter_LDADD = ../cwds/libcwds_r.la -lboost_iostreams -lboost_system

AITimer_test_SOURCES = AITimer_test.cxx
AITimer_test_CXXFLAGS = @LIBCWD_R_FLAGS@
AITimer_test_LDADD = ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

AILookupTask_test_SOURCES = AILookupTask_test.cxx
AILookupTask_test_CXXFLAGS = @LIBCWD_R_FLAGS@
AILookupTask_test_LDADD = ../events/libevents.la ../resolver-task/libresolvertask.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

AIResolver_test_SOURCES = AIResolver_test.cxx
AIResolver_test_CXXFLAGS = @LIBCWD_R_FLAGS@
AIResolver_test_LDADD = ../resolver-task/libresolvertask.la ../farmhash/src/libfarmhash.la ../resolver-task/dns/src/libdns.la ../evio/libevio.la ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

hash_test_SOURCES = hash_test.cxx
hash_test_CXXFLAGS = -O2 @LIBCWD_R_FLAGS@
hash_test_LDADD = ../farmhash/src/libfarmhash.la ../cwds/libcwds_r.la

serv_test_SOURCES = serv_test.cxx
serv_test_CXXFLAGS = -O2 @LIBCWD_R_FLAGS@
serv_test_LDADD = ../resolver-task/libresolvertask.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

proto_test_SOURCES = proto_test.cxx
proto_test_CXXFLAGS = -O2 @LIBCWD_R_FLAGS@
proto_test_LDADD = ../resolver-task/libresolvertask.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

resolver_getnameinfo_SOURCES = resolver_getnameinfo.cxx
resolver_getnameinfo_CXXFLAGS = @LIBCWD_R_FLAGS@
resolver_getnameinfo_LDADD = ../resolver-task/libresolvertask.la ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../events/libevents.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

socket_task_test_SOURCES = socket_task_test.cxx
socket_task_test_CXXFLAGS = @LIBCWD_R_FLAGS@
socket_task_test_LDADD = ../socket-task/libsockettask.la ../resolver-task/libresolvertask.la ../statefultask/libstatefultask.la ../threadpool/libthreadpool.la ../events/libevents.la ../farmhash/src/libfarmhash.la ../evio/libevio.la ../threadsafe/libthreadsafe.la ../cwds/libcwds_r.la

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
