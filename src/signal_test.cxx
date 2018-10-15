#include "sys.h"
#ifdef CWDEBUG
#include "debug.h"
#else
#define Debug(x) do { } while(0)
#define DoutEntering(x, y) do { std::cout << "Entering " << y << std::endl; } while(0)
#define Dout(x, y) do { std::cout << y << std::endl; } while(0)
namespace debug { void init() {} void init_thread() {} }
#endif
#include <iostream>
#include <atomic>
#include <thread>
#include <cstring>
#include <csignal>
#include <cassert>
#include <climits>
#include <unistd.h>
#include <semaphore.h>

sem_t tasks;

static std::atomic_int count34;
static std::atomic_int count35;
static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic_int isn't always lock free!");

// Array that stores the line number where each thread currently is.
std::array<std::atomic_int, 7> where;
// Line count per thread.
std::array<std::array<int, 2000>, 7> lines;
// pthread_t of each thread.
std::array<std::thread::native_handle_type, 7> ids;

extern "C" {

void sighandler(int nr)
{
  if (nr == 34)
    count34.fetch_add(1);
  else
    count35.fetch_add(1);
  sem_post(&tasks);
  // Find out which thread we are.
  pthread_t tid = pthread_self();
  for (unsigned int i = 0; i < ids.size(); ++i)
    if (ids[i] == tid)
      lines[i][where[i]]++;     // Keep track of on which line the thread was interrupted.
}

void sighandler_finish(int UNUSED_ARG(nr))
{
}

}

std::atomic_bool volatile running{true};

void thr7_run(int signr)
{
  Debug(debug::init_thread());
  DoutEntering(dc::notice, "thr7_run(" << signr << ")");
  for (int k = 0; k < 2000; ++k)
    for (int i = 0; i < 500; ++i)
    {
      for (int volatile j = 0; j < k; ++j)
        ;
      pthread_kill(ids[i % 7], signr + i % 2);
    }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  running = false;
  for (int k = 0; k < 20; ++k)
  {
    for (int volatile j = 0; j < 2000 + k; ++j)
      ;
    pthread_t id = ids[k % 7];
    if (id != 0)
      pthread_kill(id, signr + 2);
  }
}

std::atomic_int running_threads{0};
std::atomic_int was_here34{0};
std::atomic_int was_here35{0};
sigset_t blocked_signals;
sigset_t blocked_signr;

int const BEGIN = __LINE__;
void thrx_run(int n)
{
  DoutEntering(dc::notice, "thrx_run()");
  running_threads++;
  where[n] = __LINE__;
  while (running)
  {
    int cnt;
    where[n] = __LINE__;      // Handle SIG34.
                              while ((cnt = count34.load()) > 0)
                              {
      where[n] = __LINE__;
                                if (!count34.compare_exchange_weak(cnt, cnt - 1))
                                  continue;
      where[n] = __LINE__;      was_here34++;
                              }
    where[n] = __LINE__;      // Handle SIG35.
                              while ((cnt = count35.load()) > 0)
                              {
      where[n] = __LINE__;
                                if (!count35.compare_exchange_weak(cnt, cnt - 1))
                                  continue;
      where[n] = __LINE__;      was_here35++;
                              }
                              {
                                // Wait for more signals, if there aren't any left.
    where[n] = __LINE__;        sem_wait(&tasks);
                              }
    where[n] = __LINE__;
  }
  running_threads--;
  ids[n] = 0;
  Dout(dc::notice, "Leaving thread");
  Dout(dc::notice|flush_cf, "was_here34 = " << was_here34 << "; was_here35 = " << was_here35 << "; running_threads = " << running_threads);
}
int const END = __LINE__;

int main()
{
  Debug(debug::init());
  Dout(dc::notice, "SEM_VALUE_MAX = " << SEM_VALUE_MAX);

  sem_init(&tasks, 0, 0);

  // Add signal handlers.
  struct sigaction action;
  std::memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = sighandler;
  int signr = SIGRTMIN;
  if (sigaction(signr, &action, nullptr) == -1)
    assert(false);
  if (sigaction(signr + 1, &action, nullptr) == -1)
    assert(false);
  action.sa_handler = sighandler_finish;
  if (sigaction(signr + 2, &action, nullptr) == -1)
    assert(false);

  Dout(dc::notice, "Waiting for signal " << signr);

  // Prepare a sigset_t`s.
  sigemptyset(&blocked_signals);
  std::memcpy(&blocked_signr, &blocked_signals, sizeof(sigset_t));
  sigaddset(&blocked_signr, signr);

  std::thread thr1([](){ debug::init_thread(); thrx_run(1); });
  std::thread thr2([](){ debug::init_thread(); thrx_run(2); });
  std::thread thr3([](){ debug::init_thread(); thrx_run(3); });
  std::thread thr4([](){ debug::init_thread(); thrx_run(4); });
  std::thread thr5([](){ debug::init_thread(); thrx_run(5); });
  std::thread thr6([](){ debug::init_thread(); thrx_run(6); });

  // Store the thread ids in an array.
  ids[0] = pthread_self();
  ids[1] = thr1.native_handle();
  ids[2] = thr2.native_handle();
  ids[3] = thr3.native_handle();
  ids[4] = thr4.native_handle();
  ids[5] = thr5.native_handle();
  ids[6] = thr6.native_handle();

  // Start a thread that sends 2000000 times a signal `signr`.
  std::thread thr7([signr](){ thr7_run(signr); });

  thrx_run(0);

  thr1.join();
  thr2.join();
  thr3.join();
  thr4.join();
  thr5.join();
  thr6.join();
  thr7.join();

  Dout(dc::notice, "count34 = " << count34 << "count35 = " << count35);

  for (int line = BEGIN; line < END; ++line)
  {
    bool first = true;
    char const* separator = "";
    for (int t = 0; t < 7; ++t)
    {
      if (lines[t][line] > 0)
      {
        if (first)
        {
          std::cout << "Line #" << line << ": ";
          first = false;
        }
        std::cout << separator << '#' << t << ":" << lines[t][line];
        separator = ", ";
      }
    }
    if (!first)
      std::cout << std::endl;
  }
}
