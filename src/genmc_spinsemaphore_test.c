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

// Quick and dirty futex.
pthread_mutex_t qad_mutex;
uint32_t qad_ntokens = 0;
int qad_nwaiters = 0;

// Common.
_Atomic(uint64_t) m_word = 0;
uint64_t const tokens_mask = 0xffffffff;
uint64_t const spinner_mask = 0x100000000;
uint64_t const one_waiter = 0x200000000;
int const nwaiters_shift = 33;

int64_t delay_loop()
{
  return 0;
}

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

void* post_thread(void* param)
{
  post(1);
  post(2);
  return NULL;
}

void* wait_thread2(void* param)
{
  wait();
  return NULL;
}

void* wait_thread3(void* param)
{
  wait();
  return NULL;
}

int main()
{
  pthread_t t1, t2, t3;

  pthread_create(&t1, NULL, post_thread, NULL);
  pthread_create(&t2, NULL, wait_thread2, NULL);
  pthread_create(&t3, NULL, wait_thread3, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);

  assert(qad_nwaiters == 0);

  return 0;
}
