#include "sys.h"
#include <thread>
#include <array>
#include <chrono>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <algorithm>
#include "cwds/benchmark.h"
#include "debug.h"

int constexpr number_of_threads = 4;
int constexpr skip_cores = 2;

std::mutex cv_mutex;
std::condition_variable cv;
std::atomic_bool start;

// Pause instruction to prevent excess processor bus usage.
#define cpu_relax() asm volatile("pause\n": : :"memory")

struct Result
{
  int m_cpu;
  uint64_t m_start_cycles;
  uint64_t m_diff_cycles;

  void print(uint64_t DEBUG_ONLY(time_offset)) const
  {
    Dout(dc::notice, "Thread on CPU #" << m_cpu << " started running at t = " << (m_start_cycles - time_offset) << " and ran for " << m_diff_cycles / 3612059050.0 << " seconds.");
  }
};

struct SortResultByStartCycles
{
  bool operator()(Result const& a, Result const& b) { return a.m_start_cycles < b.m_start_cycles; }
};

std::mutex m;

void thread_main(int cpu, Result& result)
{
  Debug(NAMESPACE_DEBUG::init_thread());
  Debug(dc::notice.off());
  benchmark::Stopwatch stopwatch(cpu);

  // Wait till all threads are ready.
  {
    std::unique_lock<std::mutex> lk(cv_mutex);
    Dout(dc::notice|flush_cf, "Waiting for condition variable...");
    cv.wait(lk);
  }

  // Spinlock until we REALLY can start.
  while (!start.load(std::memory_order_relaxed))
    cpu_relax();

  stopwatch.start();

  volatile int v __attribute__ ((unused));
  for (int i = 0; i < 240000 / number_of_threads; ++i)
  {
    m.lock();
    for (int j = 0; j < 550; ++j)
      v = j;
    m.unlock();
  }

  stopwatch.stop();

  result.m_cpu = cpu;
  result.m_start_cycles = stopwatch.start_cycles();
  result.m_diff_cycles = stopwatch.diff_cycles();
  result.m_diff_cycles -= stopwatch.s_stopwatch_overhead;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  std::array<std::thread, number_of_threads> threads;
  std::array<Result, number_of_threads> results;

  {
    benchmark::Stopwatch stopwatch(0);
    stopwatch.calibrate_overhead(1000, 3);
  }

  eda::FrequencyCounter<uint64_t, 8> fc;
  bool done = false;

  for (int run = 0; run < 100; ++run)
  {
    start = false;

    // Start all threads.
    int core = 0;
    for (auto&& thread : threads)
    {
      std::thread new_thread([core, &results](){ thread_main(core, results[core / skip_cores]); });
      core += skip_cores;
      thread.swap(new_thread);
    }

    // Sleep a short while.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Notify all threads that they can start running the test.
    {
      std::unique_lock<std::mutex> lk(cv_mutex);
      cv.notify_all();
    }

    // Wait till all threads are spinning.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // Really start.
    start = true;

    // Join all threads.
    for (auto&& thread : threads)
      thread.join();

    for (auto&& result : results)
    {
      uint64_t bucket = result.m_diff_cycles / 3612059.05;      // milliseconds.
      //Dout(dc::notice, "Adding " << bucket);
      if (fc.add(bucket))
      {
        done = true;
        break;
      }
    }
    if (done)
      break;
  }
  fc.print_on(std::cout);
  if (done)
    std::cout << "Result: " << fc.result().m_cycles << std::endl;
  else
    std::cout << "Result: " << fc.average() << std::endl;
}
