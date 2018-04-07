// Compile as: g++ -std=c++14 -I../gnuplot-iostream -D_GNU_SOURCE -O2 -pthread mutex_test.cxx -lboost_iostreams -lboost_system

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
#include "gnuplot-iostream.h"

int constexpr cachelinesize = 64;
int constexpr loopsize = 1000000;
unsigned constexpr num_threads = 4;

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

extern uint64_t do_one_fetch_add(int thread);
extern uint64_t do_two_fetch_add(int thread);
extern uint64_t do_three_fetch_add(int thread);
extern uint64_t do_four_fetch_add(int thread);
extern uint64_t do_five_fetch_add(int thread);
extern uint64_t do_six_fetch_add(int thread);
extern uint64_t do_seven_fetch_add(int thread);
extern uint64_t do_eight_fetch_add(int thread);
extern uint64_t do_1dec(int thread);
extern uint64_t do_2dec(int thread);
extern uint64_t do_4dec(int thread);
extern uint64_t do_8dec(int thread);
extern uint64_t do_12dec(int thread);
extern uint64_t do_16dec(int thread);
extern uint64_t do_20dec(int thread);
extern uint64_t do_24dec(int thread);
extern uint64_t do_28dec(int thread);
extern uint64_t do_32dec(int thread);
extern uint64_t do_36dec(int thread);
extern uint64_t do_40dec(int thread);
extern uint64_t do_44dec(int thread);
extern uint64_t do_48dec(int thread);
extern uint64_t do_52dec(int thread);
extern uint64_t do_56dec(int thread);
extern uint64_t do_60dec(int thread);
extern uint64_t do_64dec(int thread);

using clock_type = std::chrono::high_resolution_clock;
using time_point = clock_type::time_point;

std::atomic_int ready;

struct Plot
{
  Gnuplot gp;
  std::string m_title;
  std::mutex m_mutex;
  std::map<std::string, std::vector<boost::tuple<double, double, double>>> m_map;
  int m_x_min;
  int m_x_max;

  void set_title(std::string title) { m_title = title; }
  void set_xrange(int x_min, int x_max) { m_x_min = x_min; m_x_max = x_max; }
  bool has_data() const { return !m_map.empty(); }

  void add_data_point(double x, double y, double dy, std::string const& description)
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_map[description].emplace_back(x, y, dy); 
  }

  void show(std::string with)
  {
    gp << "set title '" << m_title << "'\n";
    gp << "set xrange [" << m_x_min << ":" << m_x_max << "]\nset yrange [0:]\n";
    char const* separator = "plot ";
    for (auto&& e : m_map)
    {
      gp << separator << "'-' with " << with << " title '" << e.first << "'";
      separator = ", ";
    }
    gp << '\n';
    for (auto&& e : m_map)
      gp.send1d(e.second);
  }
};

Plot plot1;
Plot plot2[34];
std::mutex all_mutex;
int max_clks_all[34] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int min_clks_all[34] = { 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000 };
int max_repeats;

void benchmark(int thread, int test_nr, int repeats, std::string desc, uint64_t (*func)(int))
{
  iomutex.lock();
  std::cout << "Calling benchmark(" << thread << ", " << test_nr << ", " << repeats << ", \"" << desc << "\", func)" << std::endl;
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
    static std::atomic_int benchmarking1;
    static std::atomic_int benchmarking2;
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
      dt[i + zone] = func(thread);
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
    int mc = 2 * max_clks;
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
    clocks.push_back(clks);

    std::unique_lock<std::mutex> lk(all_mutex);
    max_clks_all[test_nr] = std::max(max_clks_all[test_nr], max_clks);
    min_clks_all[test_nr] = std::min(min_clks_all[test_nr], min_clks);
  }
  {
    std::string title = std::to_string(repeats) + " " + desc;
    std::unique_lock<std::mutex> lk(all_mutex);
    plot2[test_nr].set_xrange(min_clks_all[test_nr], max_clks_all[test_nr]);
    plot2[test_nr].set_title(title);
  }
  for (auto&& e : mv)
  {
    if (e.second.size() >= 4)
    {
      e.second.erase(e.second.begin());
      if (e.second.size() > 20)
        e.second.resize(20);
      auto mv_result = stats(e.second, 50);
      plot2[test_nr].add_data_point(e.first, mv_result.first, mv_result.second, "CPU #" + std::to_string(thread));
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

  std::unique_lock<std::mutex> lk(iomutex);
  std::cout << "===Thread #" << thread << " 99.9% confidence interval========================================================\n";
  std::cout << "Description: " << repeats << ' ' << desc << "\n";
  std::cout << "Time: " << data_ns_result.first << " ± " << data_ns_result.second << " ns.\n";
  std::cout << "Clocks: " << clocks_result.first << " ± " << clocks_result.second << ".\n";

  plot1.add_data_point((double)repeats, data_ns_result.first, data_ns_result.second, "CPU #" + std::to_string(thread) + " (ns)");
  plot1.add_data_point((double)repeats, clocks_result.first, clocks_result.second, "CPU #" + std::to_string(thread));
}

std::atomic_int count;

void run(int thread)
{
  std::cout << thread << ". Calling run(" << thread << ")\n";
  int test_nr = 0;
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 1, "dec", do_1dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 2, "dec", do_2dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 4, "dec", do_4dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 8, "dec", do_8dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 12, "dec", do_12dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 16, "dec", do_16dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 20, "dec", do_20dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 24, "dec", do_24dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 28, "dec", do_28dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 32, "dec", do_32dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 36, "dec", do_36dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 40, "dec", do_40dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 44, "dec", do_44dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 48, "dec", do_48dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 52, "dec", do_52dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 56, "dec", do_56dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 60, "dec", do_60dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
#if 1
  ++test_nr;
  benchmark(thread, test_nr, 64, "dec", do_64dec);
  count.fetch_add(1);
  while (count.load() < test_nr * num_threads);
#endif
  std::unique_lock<std::mutex> lk(iomutex);
  std::cout << "Leaving run(" << thread << ")\n";
}

int main()
{
  std::array<std::thread, num_threads> threads;
  for (int t = 0; t < num_threads; ++t)
  {
    threads[t] = std::thread{[t](){ std::this_thread::sleep_for(std::chrono::milliseconds(20)); run(t);}};
    // Pin thread to a single CPU.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(t, &cpuset);
    int rc = pthread_setaffinity_np(threads[t].native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
  }
  for (int t = 0; t < num_threads; ++t)
    threads[t].join();
  if (plot1.has_data())
  {
    plot1.set_title("Average time per number of fetch\\_add`s");
    plot1.set_xrange(0, max_repeats + 1);
    plot1.show("errorbars");
  }
  for (int n = 1; n <= 8; ++n)
    if (plot2[n].has_data())
      plot2[n].show("errorlines");
}

struct B {
  std::mutex m;
  char b[cachelinesize - sizeof(std::mutex)];
};
std::array<B, 8> m;

uint64_t do_one_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_two_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_three_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_four_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_3[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_five_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_3[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_4[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_six_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_3[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_4[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_5[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_seven_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_3[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_4[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_5[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_6[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_eight_fetch_add(int thread)
{
  uint64_t start;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  //m[thread].m.lock();
  g_count_0[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_1[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_2[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_3[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_4[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_5[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_6[thread].a.fetch_add(1, std::memory_order_seq_cst);
  g_count_7[thread].a.fetch_add(1, std::memory_order_seq_cst);
  //m[thread].m.unlock();

  uint64_t end;
  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_1dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $1, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_2dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $2, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_4dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $4, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_8dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $8, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_12dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $12, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_16dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $16, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_20dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $20, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_24dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $24, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_28dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $28, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_32dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $32, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_36dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $36, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_40dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $40, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_44dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $44, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_48dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $48, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_52dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $52, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_56dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $56, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_60dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $60, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}

uint64_t do_64dec(int thread)
{
  uint64_t start;
  uint64_t end;

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (start)
                :
                : "rdx");

  asm volatile ("movl $64, %%ecx\n\t"
                "1:\n\t"
                "decl %%ecx\n\t"
                "jnz 1b"
                :
                :
                : "ecx");

  asm volatile ("rdtsc\n\t"
                "shl $32, %%rdx\n\t"
                "or %%rdx, %0"
                : "=a" (end)
                :
                : "rdx");

  return end - start;
}
