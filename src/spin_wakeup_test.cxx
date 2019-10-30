#include "sys.h"
#include "debug.h"
#include "threadsafe/SpinSemaphore.h"
#include "utils/macros.h"
//#include "cwds/benchmark.h"
#include "cwds/gnuplot_tools.h"
#include <thread>
#include <vector>
#include <iostream>

constexpr int post_amount = 2;
constexpr int number_of_sleeper_threads = 4;
constexpr int number_of_trigger_threads = 4;
constexpr int number_of_times_to_wait = 3000000;
constexpr int number_of_times_to_wait_per_sleeper = number_of_times_to_wait / number_of_sleeper_threads;
constexpr int number_of_times_to_post_per_trigger = number_of_times_to_wait / number_of_trigger_threads / post_amount;
std::atomic<unsigned long> delay_loop = ATOMIC_VAR_INIT(0);

class SpinSemaphore : public aithreadsafe::SpinSemaphore
{
 public:
  uint64_t debug_word() const
  {
    return m_word.load(std::memory_order_relaxed);
  }

  void sanity_check() const
  {
    ASSERT(m_word == 0);
  }
};

SpinSemaphore sem;

// Count the total number of times that a thread was woken up.
std::atomic_int woken_up_count = ATOMIC_VAR_INIT(0);
std::atomic_bool go = ATOMIC_VAR_INIT(false);
std::atomic<unsigned long> slow, fast = ATOMIC_VAR_INIT(0UL);
std::atomic<unsigned int> finished_sleepers = ATOMIC_VAR_INIT(0U);

using clock_type = std::chrono::steady_clock;

struct Point
{
  std::chrono::time_point<clock_type> m_time;
  unsigned long m_delay_loop;
  uint32_t m_tokens;

  Point(unsigned long dl, uint32_t tokens) : m_time(clock_type::now()), m_delay_loop(dl), m_tokens(tokens)
  {
    ASSERT(tokens < 40);
  }
};

using points_per_sleeper_thread_t = std::vector<Point>;

// Threads that try to go to sleep, increment a counter
// when they are woken up and then go back to sleep again.
void sleepers(points_per_sleeper_thread_t& points)
{
  while (!go)
    cpu_relax();

  for (int n = 0; n < number_of_times_to_wait_per_sleeper; ++n)
  {
    DoutEntering(dc::notice, "SpinSemaphore::wait()");
    uint64_t word = sem.fast_try_wait();
    unsigned long dl;
    if ((word & SpinSemaphore::tokens_mask) == 0)
    {
      sem.slow_wait(word);
      ++slow;
      dl = delay_loop.load(std::memory_order_relaxed);
      while (dl >= post_amount && !delay_loop.compare_exchange_weak(dl, dl - post_amount, std::memory_order_relaxed))
        ;
    }
    else
    {
      dl = delay_loop.fetch_add(1, std::memory_order_relaxed);
      fast++;
    }
    woken_up_count++;
    points.emplace_back(dl, word & SpinSemaphore::tokens_mask);
  }
  ++finished_sleepers;
}

void triggers()
{
//  benchmark::Stopwatch sw;

  while (!go)
    cpu_relax();

  // Wait till all sleepers threads sleep.
//  std::this_thread::sleep_for(std::chrono::milliseconds(10));

//  uint64_t diff_cycles_sum = 0;
//  int cnt = 0;

  for (int n = 0; n < number_of_times_to_post_per_trigger; ++n)
  {
    while ((sem.debug_word() & SpinSemaphore::tokens_mask) > 8)
      cpu_relax();
//    sw.start();
    sem.post(post_amount);
//    sw.stop();

#if 0
    uint64_t dc = sw.diff_cycles();
    if (dc < 5 * 3612.05)
    {
      diff_cycles_sum += dc;
      ++cnt;
    }
#endif

#if 1
    unsigned long loop_size = delay_loop.load(std::memory_order_relaxed);
    for (unsigned long delay = 0; delay < loop_size; ++delay)
      asm volatile ("");
#else
    do
    {
      cpu_relax();
    }
    while ((sem.debug_word() >> SpinSemaphore::nwaiters_shift) < number_of_sleeper_threads - finished_sleepers);
    for (unsigned long delay = 0; delay < 10000; ++delay)
      asm volatile ("");
#endif
  }

//  std::cout << "Ran on average for " << diff_cycles_sum / 3.612059050 / cnt <<
//    " nanoseconds (with " << (number_of_times_to_post_per_trigger - cnt) << " outliers)." << std::endl;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()...");

  std::vector<std::thread> sleeper_threads;
  std::array<points_per_sleeper_thread_t, number_of_sleeper_threads> plot_data;
  std::vector<std::thread> trigger_threads;

  std::string thread_name_base = "sleeper";
  char c = '1';
  for (int n = 0; n < number_of_sleeper_threads; ++n)
    sleeper_threads.emplace_back([n, thread_name = thread_name_base + c++, &plot_data](){ Debug(NAMESPACE_DEBUG::init_thread(thread_name)); sleepers(plot_data[n]); });
  thread_name_base = "trigger";
  c = '1';
  for (int n = 0; n < number_of_trigger_threads; ++n)
    trigger_threads.emplace_back([thread_name = thread_name_base + c++](){ Debug(NAMESPACE_DEBUG::init_thread(thread_name)); triggers(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto start = clock_type::now();
  go.store(1);

  for (auto& thread : trigger_threads)
    thread.join();
  for (auto& thread : sleeper_threads)
    thread.join();
  auto stop = clock_type::now();

  //Dout(dc::notice, "woken_up_count = " << woken_up_count);
  std::cout << "woken_up_count = " << woken_up_count << std::endl;
  ASSERT(woken_up_count == number_of_times_to_wait);
  std::cout << "delay_loop = " << delay_loop << std::endl;
  std::cout << "fast = " << fast << "; slow = " << slow << std::endl;

  // Draw a plot of the data.
  //
  // There are two sets of points, (time_point, delay_loop) and (t_time_point, tokens).
  //
  // The time_point runs from start to stop.
  constexpr int nbuckets = 3000;
  auto const width = (stop - start) / nbuckets;
  std::cout << "Bucket width = " << std::chrono::duration<double, std::milli>(width).count() << " ms." << std::endl;

  using bucket_t = unsigned int;
  auto get_bucket = [start, width](clock_type::time_point tp) -> bucket_t { return (tp - start) / width; };

  std::array<std::pair<uint64_t, int>, nbuckets> delay_loop_sums;
  std::array<std::pair<uint64_t, int>, nbuckets> tokens_sums;
  delay_loop_sums.fill({0, 0});
  tokens_sums.fill({0, 0});

  for (auto& plot_data_per_thread : plot_data)
    for (auto point : plot_data_per_thread)
    {
      int bucket = get_bucket(point.m_time);
      if (bucket > nbuckets)
      {
        std::cout << "* bucket overflow!" << std::endl;
        continue;
      }

      delay_loop_sums[bucket].first += point.m_delay_loop;
      delay_loop_sums[bucket].second++;

      tokens_sums[bucket].first += point.m_tokens;
      tokens_sums[bucket].second++;
    }

  std::array<uint64_t, nbuckets> delay_loop_avgs;
  std::array<uint32_t, nbuckets> tokens_avgs;
  uint64_t max_delay_loop_avg = 0;
  uint32_t max_tokens_avg = 0;
  for (int bucket = 0; bucket < nbuckets; ++bucket)
  {
    if (delay_loop_sums[bucket].second)
    {
      delay_loop_avgs[bucket] = delay_loop_sums[bucket].first / delay_loop_sums[bucket].second;
      max_delay_loop_avg = std::max(max_delay_loop_avg, delay_loop_avgs[bucket]);
    }
    else
      delay_loop_avgs[bucket] = 0;
    if (tokens_sums[bucket].second)
    {
      tokens_avgs[bucket] = tokens_sums[bucket].first / tokens_sums[bucket].second;
      max_tokens_avg = std::max(max_tokens_avg, tokens_avgs[bucket]);
    }
    else
      tokens_avgs[bucket] = 0;
  }

  double scale = 1.0 * max_delay_loop_avg / max_tokens_avg;

  std::cout << "max_tokens_avg = " << max_tokens_avg << "; max_delay_loop_avg = " << max_delay_loop_avg << "; scale = " << scale << std::endl;

  eda::PlotHistogram plot("delay loop size progression", "time (ms)", "delay loop size", std::chrono::duration_cast<std::chrono::milliseconds>(width).count());
  std::string const delay_loop_desc = "delay\\_loop";
  std::string const tokens_desc = "tokens";
  for (int bucket = 0; bucket < nbuckets; ++bucket)
  {
    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(bucket * width).count();
    plot.add_data_point(dt_ms, delay_loop_avgs[bucket], delay_loop_desc);
    plot.add_data_point(dt_ms, scale * tokens_avgs[bucket], tokens_desc);
  }
  plot.add("set key top left");
  plot.show();

  sem.sanity_check();

  Dout(dc::notice, "Leaving main()...");
}
