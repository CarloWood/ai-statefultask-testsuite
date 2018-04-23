#ifdef CWDEBUG
#include "sys.h"
#include "debug.h"
#else
#define Debug(x) do { } while(0)
#define DoutEntering(x, y) do { std::cout << "Entering " << y << std::endl; } while(0)
#define Dout(x, y) do { std::cout << y << std::endl; } while(0)
#endif
#include <iostream>
#include <atomic>
#include <thread>
#include <cstring>
#include <csignal>
#include <cassert>
#include <unistd.h>

static std::atomic_int count;
static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic_int isn't always lock free!");

extern "C" {

void sighandler(int nr)
{
  count.fetch_add(1, std::memory_order_relaxed);
}

}

void thr1_run(pthread_t id, int signr)
{
  Debug(debug::init_thread());
  DoutEntering(dc::notice, "thr1_run(" << std::hex << id << std::dec << ")");
  for (int i = 0; i < 100000; ++i)
    pthread_kill(id, signr);
}

int main()
{
  Debug(debug::init());
  
  // Add signal handler.
  struct sigaction action;
  std::memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = sighandler;  
  int signr = SIGRTMIN;
  if (sigaction(signr, &action, nullptr) == -1)
    assert(false);

  Dout(dc::notice, "Waiting for signal " << signr);

  // Start a thread that sends 100 times a signal.
  pthread_t main_thread_id = pthread_self();
  std::thread thr1([main_thread_id, signr](){ thr1_run(main_thread_id, signr); });

  thr1.join();

  Dout(dc::notice, "count = " << count);
}
