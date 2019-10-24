// Install https://github.com/MPI-SWS/genmc
//
// Then test with:
//
// genmc -unroll=3 -- -I$REPOBASE-objdir/src genmc_spinsemaphore_test.c

// These header files are replaced by genmc (see /usr/local/include/genmc):
#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <stdatomic.h>
//#include <stdio.h>
#include <stdint.h>

// Quick and dirty futex.
pthread_mutex_t qad_mutex;
uint32_t qad_ntokens = 0;
int qad_nwaiters = 0;

// Common.
_Atomic(uint64_t) m_word = 0;
uint64_t const tokens_mask     =      0xffffffff;
uint64_t const spinner_mask    =     0x100000000;
uint64_t const futex_wake_bit  =     0x200000000;
uint64_t const futex_wake_mask =    0xfe00000000;
uint64_t const futex_woke_bit  =   0x10000000000;
uint64_t const futex_woke_mask =  0xff0000000000;
uint64_t const one_waiter      = 0x1000000000000;
int const futex_woke_shift = 40;
int const nwaiters_shift = 48;

// Util.
int futex_wait(uint32_t expected)
{
  int ret = 0;

  pthread_mutex_lock(&qad_mutex);
  {
    uint32_t word = atomic_load_explicit(&m_word, memory_order_relaxed);
    if (word == expected)
      ++qad_nwaiters;
    else
      ret = -1;   // Assume errno == EAGAIN in this case.
  }
  pthread_mutex_unlock(&qad_mutex);

  if (ret == 0)
  {
    int got_token = 0;
    while (!got_token)
    {
      pthread_mutex_lock(&qad_mutex);
      {
        if (qad_ntokens > 0)
        {
          // Atomically grab token and wake up.
          atomic_fetch_add_explicit(&m_word, futex_woke_bit, memory_order_relaxed);
          --qad_ntokens;
          --qad_nwaiters;
          got_token = 1;
        }
      }
      pthread_mutex_unlock(&qad_mutex);
    }
  }
  return ret;
}

uint32_t futex_wake(uint32_t n_threads)
{
  uint32_t woken_up;
  pthread_mutex_lock(&qad_mutex);
  {
    assert(qad_nwaiters >= qad_ntokens);
    uint32_t actual_waiters = qad_nwaiters - qad_ntokens;
    woken_up = n_threads <= actual_waiters ? n_threads : actual_waiters;
    qad_ntokens += woken_up;
  }
  pthread_mutex_unlock(&qad_mutex);
  return woken_up;
}

void slow_wait(uint64_t word);

// INCLUDES_BEGIN
#include "genmc_spinsemaphore_post.h"
#include "genmc_spinsemaphore_fast_try_wait.h"
#include "genmc_spinsemaphore_wait.h"
#include "genmc_spinsemaphore_slow_wait.hc"
// INCLUDES_END

_Atomic(int) wokeup = 0;

// We need to test:
//
// 1) Calling post(1) [or post(2)] while the number of tokens is 0.
//    1a) while having no waiters.					* p
//    1b) while having one waiter.
//        1b1) the waiter being the spinner thread.			* wp
//        1b2) the waiter not being the spinner thread.			* wwpp
//    1c) while having two waiters.
//        1c1) where on of the waiters is the spinner thread.		* wwp
//        1c2) where none of the waiters is the spinner thread.		* wwwpp
// 2) Calling post(1) [or post(2)] while there is already a token.
//    2a) while having no waiters.					* pp
//    2b) while having one waiter.
//        2b1) the waiter being the spinner thread.			* wpp
//        2b2) the waiter not being the spinner thread.			* wwppp
//    2c) while having two waiters.
//        2c1) where one of the waiters is the spinner thread.		* wwpp
//        2c2) where none of the waiters is the spinner thread.		* wwwppp
//    3d) while having three waiters.
//        3d1) where one of the waiters is the spinner thread.		* wwwpp
//
// 3) Calling wait()...
//

_Atomic(int) count = 0;

void* post_thread(void* param)
{
  int nwaiters;
  uint32_t ntokens;
  int have_spinner;
  uint64_t word;
  int futex_wakers;
  int futex_wokers;

  atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
  post(1);
  pthread_mutex_lock(&qad_mutex);
  word = atomic_load_explicit(&m_word, memory_order_relaxed);
  nwaiters = word >> nwaiters_shift;
  assert(nwaiters >= qad_nwaiters);
  ntokens = word & tokens_mask;
  have_spinner = (word & spinner_mask) ? 1 : 0;
  futex_wakers = (word & futex_wake_mask) >> 31;
  futex_wokers = (word & futex_woke_mask) >> futex_woke_shift;
  assert(!(qad_nwaiters > 0 && !have_spinner && ntokens > futex_wokers && qad_ntokens == 0 && futex_wakers == 0));
  pthread_mutex_unlock(&qad_mutex);

  atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
  post(1);
  pthread_mutex_lock(&qad_mutex);
  word = atomic_load_explicit(&m_word, memory_order_relaxed);
  nwaiters = word >> nwaiters_shift;
  assert(nwaiters >= qad_nwaiters);
  ntokens = word & tokens_mask;
  have_spinner = (word & spinner_mask) ? 1 : 0;
  futex_wakers = (word & futex_wake_mask) >> 31;
  futex_wokers = (word & futex_woke_mask) >> futex_woke_shift;
  assert(!(qad_nwaiters > 0 && !have_spinner && ntokens > futex_wokers && qad_ntokens == 0 && futex_wakers == 0));
  pthread_mutex_unlock(&qad_mutex);

#if 0
  atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
  post(1);
  pthread_mutex_lock(&qad_mutex);
  word = atomic_load_explicit(&m_word, memory_order_relaxed);
  nwaiters = word >> nwaiters_shift;
  assert(nwaiters >= qad_nwaiters);
  ntokens = word & tokens_mask;
  have_spinner = (word & spinner_mask) ? 1 : 0;
  futex_wakers = (word & futex_wake_mask) >> 31;
  assert(!(qad_nwaiters > 0 && !have_spinner && ntokens > futex_wokers && qad_ntokens == 0 && futex_wakers == 0));
  pthread_mutex_unlock(&qad_mutex);
#endif

  return NULL;
}

void* wait_thread1(void* param)
{
  wait(); // slow_wait
  int prev_count = atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
  assert(prev_count >= 1);
  return NULL;
}

void* wait_thread2(void* param)
{
  wait();
  int prev_count = atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
  assert(prev_count >= 1);
  return NULL;
}

void* wait_thread3(void* param)
{
  wait();
  int prev_count = atomic_fetch_sub_explicit(&count, 1, memory_order_relaxed);
  assert(prev_count >= 1);
  return NULL;
}

int main()
{
  pthread_t t1, t2, t3, t4;

  pthread_create(&t1, NULL, wait_thread1, NULL);
  pthread_create(&t2, NULL, wait_thread2, NULL);
//  pthread_create(&t3, NULL, wait_thread3, NULL);
  pthread_create(&t4, NULL, post_thread, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
//  pthread_join(t3, NULL);
  pthread_join(t4, NULL);

  assert(atomic_load_explicit(&count, memory_order_relaxed) == 0);

  return 0;
}
