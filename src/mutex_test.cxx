// Compile as: g++ -std=c++14 -I.. -D_GNU_SOURCE -O2 -pthread mutex_test.cxx -lboost_iostreams -lboost_system

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <array>
#include <vector>
#include <cstdint>
#include <sched.h>
#include <condition_variable>
#include <cmath>
#include <map>
#include <pthread.h>
#include <boost/tuple/tuple.hpp>
#include <boost/math/distributions/students_t.hpp>
#include "cwds/gnuplot_tools.h"

int constexpr cachelinesize = 64;
int constexpr loopsize = 100000;
unsigned constexpr num_threads = 2;

std::mutex iomutex;

std::pair<double, double> stats(std::vector<double> const& data, double confidence)
{
  unsigned Sn = data.size();
  double sum = 0;
  for (auto&& e : data)
    sum += e;
  double Sm = sum / Sn;
  sum = 0;
  for (auto&& e : data)
  {
    double d = e - Sm;
    sum += d * d;
  }
  double Sd = std::sqrt(sum / Sn);
  boost::math::students_t dist(Sn - 1);
  double T = boost::math::quantile(boost::math::complement(dist, (1.0 - confidence / 100) / 2));
  double w = T * Sd / std::sqrt(double(Sn));
  return std::make_pair(Sm, w);
}

struct alignas(cachelinesize) A {
  std::atomic_int a;
  char b[cachelinesize - sizeof(std::atomic_int)];
  A() : a(0) { }
};

std::array<A, num_threads> g_count_0;
std::array<A, num_threads> g_count_1;
std::array<A, num_threads> g_count_2;
std::array<A, num_threads> g_count_3;
std::array<A, num_threads> g_count_4;
std::array<A, num_threads> g_count_5;
std::array<A, num_threads> g_count_6;
std::array<A, num_threads> g_count_7;

static_assert((((size_t)&g_count_0) % cachelinesize) == 0, "g_count_0 not aligned on cache line.");
static_assert((((size_t)&g_count_1) % cachelinesize) == 0, "g_count_1 not aligned on cache line.");
static_assert((((size_t)&g_count_2) % cachelinesize) == 0, "g_count_2 not aligned on cache line.");
static_assert((((size_t)&g_count_3) % cachelinesize) == 0, "g_count_3 not aligned on cache line.");
static_assert((((size_t)&g_count_4) % cachelinesize) == 0, "g_count_4 not aligned on cache line.");
static_assert((((size_t)&g_count_5) % cachelinesize) == 0, "g_count_5 not aligned on cache line.");
static_assert((((size_t)&g_count_6) % cachelinesize) == 0, "g_count_6 not aligned on cache line.");
static_assert((((size_t)&g_count_7) % cachelinesize) == 0, "g_count_7 not aligned on cache line.");

extern uint64_t do_Ndec(int thread, int loop_count);

using clock_type = std::chrono::high_resolution_clock;
using time_point = clock_type::time_point;

std::atomic_int ready = ATOMIC_VAR_INIT(0);

eda::Plot plot("Number of clocks it takes to lock/unlock a mutex as function of frequency (in ns).",
          "Interval between calls to the lock/unlock pair (in ns)",
          "Time to lock and unlock a mutex (in clks)");
std::mutex all_mutex;
std::array<int, 300> max_clks_all;
std::array<int, 300> min_clks_all;
int max_repeats;

void benchmark(int thread, int test_nr, int repeats, std::string desc, uint64_t (*func)(int, int), int loop_count)
{
  iomutex.lock();
  std::cout << "Calling benchmark(" << thread << ", " << test_nr << ", " << repeats << ", \"" << desc << "\", func, " << loop_count << ")" << std::endl;
  iomutex.unlock();
  if (thread == 0)
    max_repeats = repeats;

  static int constexpr zone = cachelinesize / sizeof(uint64_t);
  std::vector<uint64_t> dt(loopsize + 2 * zone);        // Add a cacheline before and after the range that we use for this measurement.
  std::vector<double> data_ns;
  std::vector<double> clocks;
  std::map<int, std::vector<double>> mv;

  for (int j = 1; j <= 21; ++j)
  {
    // Synchronize all threads again...
    static std::atomic_int benchmarking1 = ATOMIC_VAR_INIT(0);
    static std::atomic_int benchmarking2 = ATOMIC_VAR_INIT(0);
    benchmarking1.fetch_add(1);
    while (benchmarking1.load() < num_threads)
      ;
    benchmarking2.fetch_add(1);
    while (benchmarking2.load() < num_threads)
      ;
    benchmarking1.store(0);

    // Benchmark.
    time_point start = clock_type::now();
    for (int i = 0; i < loopsize; ++i)
      dt[i + zone] = func(thread, loop_count);
    time_point end = clock_type::now();

    time_point::duration delta = end - start;

    benchmarking2.store(0);

    double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() / loopsize;
    //std::cout << thread << ". (CPU " << sched_getcpu() << ") Average time: " << ns << " ns.\n";
    data_ns.push_back(ns);

    std::map<int, int> map;
    int max_cnt = 0;
    int max_clks = 0;
    int min_clks = 1000000;
    for (int i = 0; i < loopsize; ++i)
    {
      int v = (map[dt[i + zone]] += 1);
      if (v > max_cnt)
      {
        max_cnt = v;
        max_clks = dt[i + zone];
      }
      if (dt[i + zone] < min_clks)
      {
        min_clks = dt[i + zone];
      }
    }
    int mc = std::min(1000, 5 * max_clks);
    max_clks = 0;
    for (int i = min_clks; i < mc; ++i)
    {
      if (map[i] >= max_cnt / 100)
      {
        max_clks = i;
      }
    }
    uint64_t sum = 0;
    uint64_t size = 0;
    for (auto&& e : map)
    {
      if (e.first >= min_clks && e.first <= max_clks)
      {
        mv[e.first].push_back((double)e.second);
        sum += e.first * e.second;
        size += e.second;
      }
    }
    double clks = (double)sum / size;
    //std::cout << thread << ". Clocks: " << clks << std::endl;
    clocks.push_back(clks - 39);        // 39 is the rdtsc overhead on my box.

    if (j > 1) // The first measurement is thrown away.
    {
      std::lock_guard<std::mutex> lk(all_mutex);
      max_clks_all[test_nr] = std::max(max_clks_all[test_nr], max_clks);
      min_clks_all[test_nr] = std::min(min_clks_all[test_nr], min_clks);
    }
  }
  {
    std::string title = std::to_string(repeats) + " " + desc;
    std::lock_guard<std::mutex> lk(all_mutex);
  }
  for (auto&& e : mv)
  {
    if (e.second.size() >= 4)
    {
      e.second.erase(e.second.begin());
      if (e.second.size() > 20)
        e.second.resize(20);
      auto mv_result = stats(e.second, 99);
    }
  }

  assert(data_ns.size() >= 21);
  assert(clocks.size() >= 21);
  data_ns.erase(data_ns.begin());
  clocks.erase(clocks.begin());
  data_ns.resize(20);
  clocks.resize(20);

  auto data_ns_result = stats(data_ns, 99.9);
  auto clocks_result = stats(clocks, 99.9);

  std::lock_guard<std::mutex> lk(iomutex);
  std::cout << "===Thread #" << thread << "==================================================================================\n";
  std::cout << "Description: " << repeats << ' ' << desc << "\n";
  std::cout << "Time: " << data_ns_result.first << " ± " << data_ns_result.second << " ns (99.9% confidence interval).\n";
  std::cout << "Clocks: " << clocks_result.first << " ± " << clocks_result.second << " (99% confidence interval).\n";

  plot.add_data_point(data_ns_result.first, clocks_result.first, clocks_result.second, "CPU #" + std::to_string(thread));
  //plot.add_data_point((double)repeats, clocks_result.first, clocks_result.second, "CPU #" + std::to_string(thread) + "(clks)");
}

std::atomic_int count = ATOMIC_VAR_INIT(0);
int test_nr2 = 0;

void run(int thread)
{
  std::cout << thread << ". Calling run(" << thread << ")\n";
  int test_nr = 0;
#if 1
  for (int loop_count = 1000; loop_count >= 30; loop_count -= 2)
  {
    ++test_nr;
    benchmark(thread, test_nr, loop_count, "dec", do_Ndec, loop_count);
    count.fetch_add(1);
    while (count.load() < test_nr * num_threads);
  }
#endif
  if (thread == 0)
    test_nr2 = test_nr;
  std::lock_guard<std::mutex> lk(iomutex);
  std::cout << "Leaving run(" << thread << ")\n";
}

int main()
{
  for (int i = 0; i < min_clks_all.size(); ++i)
    min_clks_all[i] = 1000000;
  std::array<std::thread, num_threads> threads;
  for (int t = 0; t < num_threads; ++t)
  {
    threads[t] = std::thread{[t](){ std::this_thread::sleep_for(std::chrono::milliseconds(20)); run(t);}};
    // Pin thread to a single CPU.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2 * t, &cpuset);
    int rc = pthread_setaffinity_np(threads[t].native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
  }
  for (int t = 0; t < num_threads; ++t)
    threads[t].join();
  if (plot.has_data())
  {
    //plot.set_xrange(125, 150 /*max_repeats + 1*/);
    //plot.set_yrange(150, 250 /*max_repeats + 1*/);
    plot.show("errorbars");
  }
}

struct B {
  std::mutex m;
  char b[cachelinesize - sizeof(std::mutex)];
};
std::array<B, 8> m;

uint64_t do_Ndec(int thread, int loop_count)
{
  uint64_t start;
  uint64_t end;
  int __d0;

  asm volatile ("lfence\n\t"
                "rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "%rdx");

  m[0].m.lock();
  m[0].m.unlock();

  asm volatile ("rdtscp\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "%rdx", "%rcx");

  asm volatile ("\n"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                : "=c" (__d0)
                : "c" (loop_count - thread)
                : "cc");

  return end - start;
}
