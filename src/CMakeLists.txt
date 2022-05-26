include(AICxxProject)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem iostreams program_options system)
find_package(farmhash REQUIRED)

# Use --disable-new-dtags when linking executables. This is to avoid setting the RUNPATH
# entry in the dynamic section of executables when using -rpath. As a result the path
# set with -rpath takes precedence over LD_LIBRARY_PATH.
#
# In general, the CORRECT way to deal with this is to make sure that you do not have
# libraries installed in your LD_LIBRARY_PATH that you don't want to link with (aka,
# that are installed in gitache!).
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

add_executable(pointer_storage_test pointer_storage_test.cxx)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(pointer_storage_test PRIVATE "-O2")
endif()
target_link_libraries(pointer_storage_test PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(threadpool_yield_test threadpool_yield_test.cxx)
target_link_libraries(threadpool_yield_test PRIVATE AICxx::helloworld-task ${AICXX_OBJECTS_LIST})

add_executable(timer_threadsafety_test timer_threadsafety_test.cxx)
target_link_libraries(timer_threadsafety_test PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(slow_down_test slow_down_test.cxx)
target_link_libraries(slow_down_test PRIVATE AICxx::helloworld-task ${AICXX_OBJECTS_LIST})

add_executable(wait_signal_test2 wait_signal_test2.cxx)
target_link_libraries(wait_signal_test2 PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(wait_signal_test wait_signal_test.cxx)
target_link_libraries(wait_signal_test PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(proxy_test proxy_test.cxx)
target_link_libraries(proxy_test PRIVATE AICxx::socket-task AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})

add_executable(rewrite_header rewrite_header.cxx)
target_link_libraries(rewrite_header PRIVATE AICxx::cwds -lstdc++fs Boost::program_options)

add_executable(minimal minimal.cxx)
target_link_libraries(minimal PRIVATE AICxx::helloworld-task ${AICXX_OBJECTS_LIST})

add_executable(delay_loop_test delay_loop_test.cxx)
target_link_libraries(delay_loop_test PRIVATE AICxx::threadsafe AICxx::utils AICxx::cwds Boost::iostreams Boost::system)

add_executable(spin_wakeup_test spin_wakeup_test.cxx)
target_link_libraries(spin_wakeup_test PRIVATE AICxx::threadsafe AICxx::utils AICxx::cwds Boost::iostreams Boost::system)

add_executable(semaphore_test semaphore_test.cxx)
target_link_libraries(semaphore_test PRIVATE AICxx::threadsafe AICxx::utils AICxx::cwds)

add_executable(AIStatefulTaskMutex_test AIStatefulTaskMutex_test.cxx)
target_link_libraries(AIStatefulTaskMutex_test PRIVATE ${AICXX_OBJECTS_LIST})
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(AIStatefulTaskMutex_test PRIVATE "-O2")
endif()

add_executable(FileLock_test FileLock_test.cxx)
target_link_libraries(FileLock_test PRIVATE AICxx::socket-task AICxx::resolver-task dns::dns AICxx::filelock-task ${AICXX_OBJECTS_LIST} Boost::iostreams Boost::filesystem Boost::system)

add_executable(helloworld helloworld.cxx)
target_link_libraries(helloworld PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(fibonacci fibonacci.cxx)
target_link_libraries(fibonacci PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(fiboquick fiboquick.cxx)

add_executable(filelock filelock.cxx)
target_link_libraries(filelock PRIVATE AICxx::cwds)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(filelock PRIVATE "-O3")
endif()

add_executable(runthread runthread.cxx)
target_link_libraries(runthread PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(function function.cxx)
target_link_libraries(function PRIVATE AICxx::cwds)

add_executable(objectqueue objectqueue.cxx)
target_link_libraries(objectqueue PRIVATE AICxx::cwds)

add_executable(threadpool threadpool.cxx)
target_link_libraries(threadpool PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(cv_wait cv_wait.cxx)
target_link_libraries(cv_wait Threads::Threads)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(cv_wait PRIVATE "-O3")
endif()

add_executable(timer_test timer_test.cxx)
target_link_libraries(timer_test PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(timer_thread timer_thread.cxx)
target_link_libraries(timer_thread PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(signal_test signal_test.cxx)
target_link_libraries(signal_test PRIVATE AICxx::threadsafe AICxx::utils AICxx::cwds)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(signal_test PRIVATE "-O3")
endif()

add_executable(benchmark benchmark.cxx)
target_link_libraries(benchmark PRIVATE AICxx::cwds Boost::iostreams Boost::system Boost::filesystem)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(benchmark PRIVATE "-O3")
endif()

add_executable(mutex_benchmark mutex_benchmark.cxx)
target_link_libraries(mutex_benchmark PRIVATE AICxx::cwds)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(mutex_benchmark PRIVATE "-O3")
endif()

add_executable(test_frequency_counter test_frequency_counter.cxx)
target_link_libraries(test_frequency_counter PRIVATE AICxx::cwds Boost::iostreams Boost::system)

add_executable(AITimer_test AITimer_test.cxx)
target_link_libraries(AITimer_test PRIVATE ${AICXX_OBJECTS_LIST})

add_executable(AILookupTask_test AILookupTask_test.cxx)
target_link_libraries(AILookupTask_test PRIVATE AICxx::events AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})

add_executable(AIResolver_test AIResolver_test.cxx)
target_link_libraries(AIResolver_test PRIVATE AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})

add_executable(hash_test hash_test.cxx)
target_link_libraries(hash_test PRIVATE farmhash::farmhash AICxx::cwds)
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(hash_test PRIVATE "-O2")
endif()

add_executable(serv_test serv_test.cxx)
target_link_libraries(serv_test PRIVATE AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(serv_test PRIVATE "-O2")
endif()

add_executable(proto_test proto_test.cxx)
target_link_libraries(proto_test PRIVATE AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})
if (CW_BUILD_TYPE_IS_DEBUG)
  target_compile_options(proto_test PRIVATE "-O2")
endif()

add_executable(resolver_getnameinfo resolver_getnameinfo.cxx)
target_link_libraries(resolver_getnameinfo PRIVATE AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})

add_executable(socket_task_test socket_task_test.cxx)
target_link_libraries(socket_task_test PRIVATE AICxx::socket-task AICxx::resolver-task dns::dns ${AICXX_OBJECTS_LIST})

# --------------- Maintainer's Section

set(GENMC_H genmc_spinsemaphore_post.h genmc_spinsemaphore_fast_try_wait.h genmc_spinsemaphore_wait.h)
set(GENMC_HC genmc_spinsemaphore_slow_wait.hc)

foreach (h_file ${GENMC_H})
  string(REPLACE ".h" ".awk" awk_file ${h_file})
  add_custom_command(OUTPUT ${h_file}
    COMMAND env AWKPATH="${CMAKE_CURRENT_SOURCE_DIR}" gawk -f "${CMAKE_CURRENT_SOURCE_DIR}/${awk_file}"
        "${top_srcdir}/threadsafe/SpinSemaphore.h" > ${h_file}
  )
endforeach ()

foreach (hc_file ${GENMC_HC})
  string(REPLACE ".hc" ".awk" awk_file ${hc_file})
  add_custom_command(OUTPUT ${hc_file}
    COMMAND env AWKPATH="${CMAKE_CURRENT_SOURCE_DIR}" gawk -f "${CMAKE_CURRENT_SOURCE_DIR}/${awk_file}"
        "${top_srcdir}/threadsafe/SpinSemaphore.cxx" > ${hc_file}
  )
endforeach ()

add_custom_target(genmc
  DEPENDS genmc_spinsemaphore_test.c ${GENMC_H} ${GENMC_HC}
  COMMENT Running genmc...
  COMMAND genmc -unroll=5 -- -std=c11 -I${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/genmc_spinsemaphore_test.c
)

add_custom_target(genmc_all
  DEPENDS genmc_spinsemaphore_test.c ${GENMC_H} ${GENMC_HC}
  BYPRODUCTS genmc_all.c
  COMMAND grep -B 1000 'INCLUDES_BEGIN' ${CMAKE_CURRENT_SOURCE_DIR}/genmc_spinsemaphore_test.c > ${CMAKE_CURRENT_SOURCE_DIR}/genmc_all.c
  COMMAND cat ${GENMC_H} ${GENMC_HC} >> ${CMAKE_CURRENT_SOURCE_DIR}/genmc_all.c
  COMMAND grep -A 1000 'INCLUDES_END' ${CMAKE_CURRENT_SOURCE_DIR}/genmc_spinsemaphore_test.c >> ${CMAKE_CURRENT_SOURCE_DIR}/genmc_all.c
  COMMENT Running genmc on genmc_all.c
  COMMAND genmc -unroll=5 -pretty-print-exec-graphs -print-error-trace -- -std=c11 -I${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/genmc_all.c
)
