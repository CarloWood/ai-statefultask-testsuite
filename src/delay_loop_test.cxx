#include "sys.h"
#include "debug.h"
#include "threadsafe/SpinSemaphore.h"
#include "utils/DelayLoopCalibration.h"
#include "utils/cpu_relax.h"
#include <atomic>
#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;

class SpinSemaphoreCalibration : public aithreadsafe::SpinSemaphore
{
 public:
  using clock_type = std::chrono::steady_clock;
  using ms_type = std::chrono::duration<double, std::milli>;            // Floating point representation of a duration in ms.
  static constexpr ms_type one_ms = 1ms;

  struct Point
  {
    int loop_size;
    ms_type delay;

    friend std::ostream& operator<<(std::ostream& os, Point const& point)
    {
      os << "Point{loop_size:" << point.loop_size << ", delay:" << point.delay.count() << "}";
      return os;
    }
  };

 private:
  std::array<Point, 2> m_points;

 public:
  clock_type::duration do_delay_loop(int delay_loop_size, int inner_loop_size)
  {
    // Measure eight times the duration of this delay loop.
    std::array<clock_type::duration, 8> measurements;
    for (unsigned int m = 0; m < measurements.size(); ++m)
    {
      auto start = clock_type::now();
      for (int i  = 0; i < delay_loop_size; ++i)
      {
        cpu_relax();
        asm volatile ("");
        if ((m_word.load(std::memory_order_relaxed) & tokens_mask) != 0)
          break;
        for (int j = 0; j < inner_loop_size; ++j)
          asm volatile ("");
      }
      auto stop = clock_type::now();
      measurements[m] = stop - start;
    }
    // Return the average of the five smallest values that where measured.
    std::sort(measurements.begin(), measurements.end());
    auto sum = measurements[0];
    for (int i = 1; i < 5; ++i)
      sum += measurements[i];
    return sum / 5;
  }

  int improve_loop_size();
  void calibrate();
};

int SpinSemaphoreCalibration::improve_loop_size()
{
  DoutEntering(dc::notice, "SpinSemaphoreCalibration::improve_loop_size()");
  // Which point is further away from 1 millisecond?
  Dout(dc::notice, "m_points[0] = " << m_points[0] << ", m_points[1] = " << m_points[1]);
  int furthest_away = std::abs((m_points[0].delay - one_ms).count()) > std::abs((m_points[1].delay - one_ms).count()) ? 0 : 1;
  Dout(dc::notice, "furthest_away = " << furthest_away);
  int new_loop_size = m_points[0].loop_size +
    std::round((m_points[1].loop_size - m_points[0].loop_size) * ((one_ms - m_points[0].delay) / (m_points[1].delay - m_points[0].delay)));
  if (new_loop_size < 0)
    new_loop_size = 0;
  if (new_loop_size == m_points[1 - furthest_away].loop_size)
  {
    if (new_loop_size == 0)
      new_loop_size = m_points[furthest_away].loop_size + 1;
    else if (m_points[1 - furthest_away].delay < one_ms)
      ++new_loop_size;
    else
      --new_loop_size;
  }
  else if (new_loop_size > m_points[furthest_away].loop_size)
    new_loop_size = std::min(m_points[furthest_away].loop_size * 2 + 16, new_loop_size);
  m_points[furthest_away] = Point{ new_loop_size, do_delay_loop(100000, new_loop_size) };
  return furthest_away;
}

void SpinSemaphoreCalibration::calibrate()
{
  m_points[0] = Point{ 0, do_delay_loop(100000, 0) };
  m_points[1] = Point{ 1, do_delay_loop(100000, 1) };

  std::vector<Point> longer_than_one_ms;
  int low = 0;
  do
  {
    int replaced = improve_loop_size();
    if (m_points[replaced].delay > one_ms)
    {
      longer_than_one_ms.push_back(m_points[replaced]);
      low = 0;
    }
    else
      ++low;
  }
  while (low < 5 && longer_than_one_ms.size() < 5);
  std::sort(longer_than_one_ms.begin(), longer_than_one_ms.end(), [](Point const& p1, Point const& p2){ return p1.delay < p2.delay; });
  Dout(dc::notice, longer_than_one_ms[0]);
}

constexpr double goal = 1.0;                    // Total time of delay loop in ms.
constexpr double time_per_loop = 1e-4;          // Shortest time between reads of atomic in loop, in ms (i.e. 100 ns).
constexpr int max_ols = goal / time_per_loop;
constexpr int min_ols = max_ols / 4;
std::atomic<uint64_t> m_word;
constexpr uint64_t tokens_mask = 0xffffffff;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  auto delay_loop = [](unsigned int outer_loop_size, unsigned int inner_loop_size) {
    unsigned int i = outer_loop_size;
    do
    {
      cpu_relax();
      if ((m_word.load(std::memory_order_relaxed) & tokens_mask) != 0)
        break;
      for (int j = inner_loop_size; j != 0; --j)
        asm volatile ("");
    }
    while (--i != 0);
  };

  unsigned int ils, ols;
  // This requires C++17.
  utils::DelayLoopCalibration fixed_ols_delay_loop([delay_loop, &ols](unsigned int ils){ return delay_loop(ols, ils); });
  utils::DelayLoopCalibration fixed_ils_delay_loop([delay_loop, &ils](unsigned int ols){ return delay_loop(ols, ils); });

  constexpr unsigned int prefered_minimum_ils = 10;
  ils = prefered_minimum_ils;
  ols = fixed_ils_delay_loop.peak_detect(goal, false);
  Dout(dc::notice, "ols (with ils = " << ils << ") = " << ols);

  if (ols < min_ols)
  {
    // Fix ols at 10000.
    ols = min_ols;
    // Find the smallest value of ils < prefered_minimum_ils that still gives a delay time of at least goal.
    for (ils = 0; ils < prefered_minimum_ils; ++ils)
      if (fixed_ols_delay_loop.avg_of(ils) > goal)
        break;
    Dout(dc::notice, "ils (with ols = " << ols << ") = " << ils);
  }
  else if (ols > max_ols)
  {
    // Fix ols at 90% of the maximum value.
    ols = 0.9 * max_ols;
    ils = fixed_ols_delay_loop.search_lowest_of(20, goal);
    Dout(dc::notice, "ils (with ols = " << ols << ") = " << ils);
  }
  else
  {
    // Assuming that ils behaves linear (it actually might, under 10), namely
    //
    //   goal = alpha * ols * (beta + gamma * ils),
    //
    // we know that multiplying ils with a factor ols / max_ols and setting ols
    // to max_ols we'd get a delay of:
    //
    //   alpha * max_ols * (beta + gamma * ils * (ols / max_ols)) = goal + alpha * beta * (max_ols - ols)
    //
    // So, the delay would become larger than goal and the ols corresponding
    // to goal will thus be smaller than max_ols.
    //
    // To speed up the search for the smallest ils such that ols is still less than max_ols,
    // do this for goal/10 and ols/10.

    unsigned int prev_ils = ils;
    ils = (1.0 * ols / max_ols) * ils;
    while (ils < prev_ils)
    {
      // Find a corresponding ols.
      ols = fixed_ils_delay_loop.peak_detect(goal / 10.0, false /*, "Delay with goal 0.1 ms and ils = " + std::to_string(ils)*/);
      Dout(dc::notice, "ols (with ils = " << ils << ") = " << ols << " (for a delay of 0.1 * goal).");
      if (ols > min_ols)
      {
        ++ils;
        break;
      }
      prev_ils = ils;
      ils = (10.0 * ols / max_ols) * ils;       // Because goal was devided by 10, ols is a factor of 10 too small.
    }
  }
  // Finally, find the best ols with this ils.
  ols = fixed_ils_delay_loop.peak_detect(goal, false, "Delay with goal 1 ms and ils = " + std::to_string(ils));
  Dout(dc::notice, "ols (with ils = " << ils << ") = " << ols);

  for (int i = 0; i < 10; ++i)
    Dout(dc::notice, "Delay loop: " << fixed_ils_delay_loop.avg_of(ols) << " ms.");

#if 0 //def CWDEBUG
  double const cpu_frequency = 3612059050.0;      // In cycles per second.
  int const cpu = 0;                              // The CPU to run on.
  size_t const loopsize = 1;                      // We'll be measing the number of clock cylces needed for this many iterations of the test code.
  size_t const minimum_of = 3;                    // All but the fastest measurement of this many measurements are thrown away (3 is normally enough).
  int const nk = 3;                               // The number of buckets of FrequencyCounter (with the highest counts) that are averaged over.

  Dout(dc::notice|flush_cf|continued_cf, "Delay Loop: using ols = " << ols << " and ils = " << ils << ". Measured delay loop... ");
  benchmark::Stopwatch stopwatch(cpu);

  // Calibrate Stopwatch overhead.
  stopwatch.calibrate_overhead(loopsize, minimum_of);

  auto result = stopwatch.measure<nk>(loopsize, [delay_loop, ols, ils]() mutable {
      asm volatile ("");

      // Code under test.
      delay_loop(ols, ils);

      asm volatile ("");
  }, minimum_of);

  Dout(dc::finish, (result / cpu_frequency * 1e3 / loopsize) << " ms [measured " << result << " clocks].");
#endif


#if 0
  SpinSemaphoreCalibration sem;
  sem.calibrate();

  Dout(dc::notice, "Calibration result: delay_loop_size = " << sem.delay_loop_size() << "; inner_loop_size = " << sem.inner_loop_size() << ".");
#endif
}
