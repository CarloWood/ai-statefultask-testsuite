#include "sys.h"
#include "debug.h"
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <thread>
#include "cwds/benchmark.h"
#include "Plot.h"

int constexpr loopsize = 100;

using clock_type = std::chrono::high_resolution_clock;
using time_point = clock_type::time_point;

namespace benchmark {

enum show_nt
{
  hide_calibration_graph = 0x1,
  hide_delta_graphs = 0x2
};

template<typename T>
class MinAvgMax
{
 private:
  T m_min;
  T m_max;
  T m_sum;
  size_t m_cnt;

 public:
  T min() const { return m_min; }
  T max() const { return m_max; }
  T avg() const { return m_sum / m_cnt; }

  MinAvgMax() : m_min(std::numeric_limits<T>::max()), m_max(-std::numeric_limits<T>::max()), m_sum(0.0), m_cnt(0) { }

  void data_point(T data)
  {
    if (data < m_min)
      m_min = data;
    if (data >m_max)
      m_max = data;
    m_sum += data;
    ++m_cnt;
  }

  size_t count() const { return m_cnt; }

  friend std::ostream& operator<<(std::ostream& os, MinAvgMax<T> const& mma)
  {
    os << mma.m_min << '/' << mma.avg() << '/' << mma.m_max;
    return os;
  }
};

struct Data
{
  double min;
  uint64_t delta1;
  uint64_t delta2;
  int bin;
  Data(double min_, uint64_t delta1_, uint64_t delta2_) :
    min(min_), delta1(delta1_), delta2(delta2_) { }
  friend std::ostream& operator<<(std::ostream& os, Data const& data)
  {
    os << data.min << ", " << data.delta1 << ", " << data.delta2;
    return os;
  }
};

class Benchmark
{
 private:
  unsigned int m_cpu_nr;
  double m_now_offset;
  int m_delta;
  double m_cycles_per_ns;
  Stopwatch* m_stopwatch1;
  Stopwatch* m_stopwatch2;

 public:
  void calibrate_now_offset(bool show);
  void calibrate_delta(bool show);
  void calibrate_cycles_per_ns();

  double now_offset() const { return m_now_offset; }
  int delta() const { return m_delta; }
  std::string now_offset_str(int precision) { std::ostringstream ss; ss << std::fixed << std::setprecision(precision) << m_now_offset; return ss.str(); }
  double cycles_per_ns() const { return m_cycles_per_ns; }

  Benchmark(unsigned int cpu_nr, int hide_graphs = 0) : m_cpu_nr(cpu_nr), m_now_offset(0), m_delta(0)
  {
    m_stopwatch1 = new Stopwatch(m_cpu_nr);
    m_stopwatch2 = new Stopwatch(m_cpu_nr);
    calibrate_now_offset(!(hide_graphs & hide_calibration_graph));
    calibrate_delta(!(hide_graphs & hide_delta_graphs));
    calibrate_cycles_per_ns();
  }

  Benchmark(unsigned int cpu_nr, double now_offset = 0.0, int delta = 0, double cycles_per_ns = 0.0, int hide_graphs = 0) :
      m_cpu_nr(cpu_nr), m_now_offset(now_offset), m_delta(delta), m_cycles_per_ns(cycles_per_ns)
  {
    m_stopwatch1 = new Stopwatch(m_cpu_nr);
    m_stopwatch2 = new Stopwatch(m_cpu_nr);
    if (now_offset == 0.0)
      calibrate_now_offset(!(hide_graphs & hide_calibration_graph));
    if (delta == 0)
      calibrate_delta(!(hide_graphs & hide_delta_graphs));
    if (cycles_per_ns == 0.0)
      calibrate_cycles_per_ns();
  }

  ~Benchmark()
  {
    delete m_stopwatch2;
    delete m_stopwatch1;
  }

  int get_minimum_of(int number_of_runs, std::function<void()> test);
};

void Benchmark::calibrate_now_offset(bool show)
{
  // Make an educated guess about the offset between two calls to clock_type::now().
  int constexpr stop_count = 1000000;          // With a smaller value there is a change that we'll measure 142 ns instead of 141 ns.
  unsigned int peak_nsi = 1000;               // 1 microsecond would be insanely large.
  bool done = false;
  while (!done)
  {
    std::vector<int> d(4 * peak_nsi, 0);
    double sum = 0;
    int cnt = 0;
    Dout(dc::notice, "Calibrating with peak_nsi = " << peak_nsi  << " ns.");
    for (;;)
    {
      // Warm up cache.
      m_stopwatch1->prefetch();
      m_stopwatch2->prefetch();
      time_point start = clock_type::now();
      time_point end = clock_type::now();

      // Measurement 1.
      m_stopwatch1->start();
      start = clock_type::now();
      m_stopwatch1->stop();

      // Measurement 2.
      m_stopwatch2->start();
      end = clock_type::now();
      m_stopwatch2->stop();

      time_point::duration delta = end - start;
      unsigned int nsi = std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count();
      if (nsi < 4 * peak_nsi)
      {
        if ((d[nsi] += 1) == stop_count)
        {
          if (peak_nsi == nsi)
            done = true;
          peak_nsi = nsi;
          break;
        }
        sum += nsi;
        ++cnt;
      }
    }
    m_now_offset = sum / cnt;
    Dout(dc::notice, "avg/peak: " << m_now_offset << "/" << peak_nsi << " [in ns].");

    if (done && show)
    {
      std::ostringstream ss;
      ss << "Average overhead: " << std::fixed << std::setprecision(1) << m_now_offset << " ns.";
      Plot plot("Calibration of overhead of calls to std::chrono::high\\_resolution\\_clock::now()",
                "Overhead between two calls to now() (in ns)",
                "Number of times measured (count)");
      plot.add("set boxwidth 0.9");
      plot.add("set style fill solid 0.5");
      plot.add("set xtics 5 rotate");
      plot.add("set mxtics 5");
      plot.add("set tics out");
      plot.add("unset key");
      plot.add("set obj 4 rect at 44.5,9000000 size 10,1000000");
      plot.add("set label 4 at 39.5,9000000 \"" + ss.str() + "\" left offset 1,.5");

      unsigned int nsi_min = 100;
      unsigned int nsi_max = 0;
      for (unsigned int nsi = 0; nsi < d.size(); ++nsi)
      {
        if (d[nsi] > stop_count / 2000)
        {
          nsi_min = std::min(nsi_min, nsi);
          nsi_max = std::max(nsi_max, nsi);
        }
        plot.add_data_point(nsi, d[nsi], 0, "clock\\_type::now() x 2");
      }
      plot.set_xrange(nsi_min - 1, nsi_max + 1);
      plot.set_header("smooth freq");
      plot.show("boxes");
    }
  }
}

void Benchmark::calibrate_delta(bool show)
{
  double int_now_offset = (int)now_offset();   // Lets use an integer value for now.

  for (;;)
  {
    std::vector<Data> data;
    {
      time_point start;
      time_point end;
      __builtin_prefetch(&start, 1);
      __builtin_prefetch(&end, 1);

      int constexpr number_of_data_points = 100000;
      int constexpr mma_count = 20;
      Dout(dc::notice|flush_cf|continued_cf, "Inner loop size calibration... ");
      while (data.size() < number_of_data_points)
      {
        MinAvgMax<double> mma;
        uint64_t delta1;
        uint64_t delta2;

        for (int i = 0; i < mma_count; ++i)
        {
          // Prefetch stuff.
          m_stopwatch1->prefetch();
          m_stopwatch2->prefetch();
          start = clock_type::now();
          end = clock_type::now();

          // Actual measurement.
          m_stopwatch1->start();
          start = clock_type::now();
          m_stopwatch1->stop();
          m_stopwatch2->start();
          end = clock_type::now();
          m_stopwatch2->stop();

          time_point::duration delta = end - start;
          double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() - int_now_offset;
          mma.data_point(ns);
          if (ns == mma.min())
          {
            delta1 = m_stopwatch1->diff_cycles();
            delta2 = m_stopwatch2->diff_cycles();
          }
        }
        // Only store the lowest measured value of mma_count measurements.
        data.push_back(Data(mma.min(), delta1, delta2));
      }
      Dout(dc::finish|flush_cf, "done");
    }

    // Find the lowest measurement and maximum minimum measurement.
    MinAvgMax<double> mma;
    MinAvgMax<int> mma_delta1;
    MinAvgMax<int> mma_delta2;
    Dout(dc::notice, "Number of data points: " << data.size());
    for (auto&& d : data)
    {
      mma.data_point(d.min);
      mma_delta1.data_point(d.delta1);
      mma_delta2.data_point(d.delta2);
    }
    int low = std::lround(mma.min());
    int high = std::lround(mma.max());
    int width = high - low + 1;
    Dout(dc::notice, "Minimum values range " << mma);

    Plot plot1("Histogram delta1", "delta1", "count");
    int best_delta1;
    {
      std::vector<int> count(mma_delta1.max() + 1 ,0);
      int max_count = 0;
      for (int c = 0; c < width; ++c)
      {
        for (auto&& d : data)
        {
          int bin = std::lround(d.min - low);
          if (bin == c && d.delta1 < 300)
          {
            plot1.add_data_point(d.delta1, count[d.delta1]++, bin, "data");
            if (count[d.delta1] > max_count)
            {
              max_count = count[d.delta1];
              best_delta1 = d.delta1;
            }
          }
        }
      }
      plot1.add("set xtics 10");
      plot1.add("set mxtics 10");
      plot1.add("set tics out");
      plot1.add("unset key");
      plot1.add("set xrange [" + std::to_string(best_delta1 - 10) + ":" + std::to_string(best_delta1 + 10) + "]");

      if (max_count < 40000)
      {
        Dout(dc::notice, "Delta calibration failed, max_count delta1 = " << max_count << ". Retrying...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
    }

    Plot plot2("Histogram delta2", "delta2", "count");
    int best_delta2;
    {
      std::vector<int> count(mma_delta2.max() + 1 ,0);
      int max_count = 0;
      for (int c = 0; c < width; ++c)
      {
        for (auto&& d : data)
        {
          int bin = std::lround(d.min - low);
          if (bin == c && d.delta2 < 300)
          {
            plot2.add_data_point(d.delta2, count[d.delta2]++, bin, "data");
            if (count[d.delta1] > max_count)
            {
              max_count = count[d.delta2];
              best_delta2 = d.delta2;
            }
          }
        }
      }
      plot2.add("set xtics 10");
      plot2.add("set mxtics 10");
      plot2.add("set tics out");
      plot2.add("unset key");
      plot2.add("set xrange [" + std::to_string(best_delta2 - 10) + ":" + std::to_string(best_delta2 + 10) + "]");

      if (max_count < 30000 || best_delta2 != best_delta1)
      {
        if (max_count < 30000)
          Dout(dc::notice, "Delta calibration failed, max_count delta2 = " << max_count << ". Retrying...");
        else if (best_delta2 != best_delta1)
          Dout(dc::notice, "Delta calibration failed, best_delta1 (" << best_delta1 << ") != best_delta2 (" << best_delta2 << "). Retrying...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
    }

    if (show)
    {
      plot1.show("points palette");
      plot2.show("points palette");
    }

    m_delta = best_delta1;
    break;
  }

  Dout(dc::notice, "Optimal number of clocks: " << m_delta);
}

void Benchmark::calibrate_cycles_per_ns()
{
  int constexpr mma_count = 20;
  std::vector<int> delta1_count(2 * m_delta, 0);
  std::vector<int> delta2_count(2 * m_delta, 0);
  uint64_t target_delta1 = 0;
  uint64_t target_delta2 = 0;

  MinAvgMax<double> mma_delta;

  time_point start;
  time_point end;

  while (1)
  {
    MinAvgMax<double> mma;
    uint64_t clocks;
    uint64_t delta1;
    uint64_t delta2;
    uint64_t delta3;

    for (int i = 0; i < mma_count; ++i)
    {
      // Warm up cache.
      m_stopwatch1->prefetch();
      start = clock_type::now();
      start = clock_type::now();

      // Do measurement 1.
      m_stopwatch1->start();
      start = clock_type::now();
      m_stopwatch1->stop();

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      for (int j = 0; j < 10; ++j)
      {
        // Warm up cache.
        m_stopwatch1->prefetch();
        m_stopwatch2->prefetch();
        end = clock_type::now();

        m_stopwatch2->start();
        end = clock_type::now();
        m_stopwatch2->stop();
      }

      // Do measurement 2.
      m_stopwatch2->start();
      end = clock_type::now();
      m_stopwatch2->stop();

      time_point::duration delta = end - start;
      double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() - m_now_offset;
      mma.data_point(ns);
      if (ns == mma.min())
      {
        delta1 = m_stopwatch1->diff_cycles();
        delta2 = m_stopwatch2->diff_cycles();
        delta3 = m_stopwatch2->stop_cycles() - m_stopwatch1->stop_cycles();
        clocks = std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count();
      }
    }
    if (target_delta1 - 1 <= delta1 && delta1 <= target_delta1 + 1 && target_delta2 - 1 <= delta2 && delta2 <= target_delta2 + 1)
    {
      Dout(dc::notice, delta3 << " / " << clocks << " = " << std::fixed << std::setprecision(9) << (double)delta3 / clocks);
      mma_delta.data_point((double)delta3 / clocks);
      if (mma_delta.count() == 20)
        break;
    }
    else if (target_delta1 == 0 || target_delta2 == 0)
    {
      Dout(dc::notice, "delta1 = " << delta1 << ", delta2 = " << delta2 << "; " << delta3 << " / " << clocks << " = " << std::fixed << std::setprecision(9) << (double)delta3 / clocks);
      if ((int)delta1 < 2 * m_delta)
      {
        if (++delta1_count[delta1] == 10)
          target_delta1 = delta1;
      }
      if ((int)delta2 < 2 * m_delta)
      {
        if (++delta2_count[delta2] == 10)
          target_delta2 = delta2;
      }
    }
    else
    {
      Dout(dc::notice, "Rejected; delta1 = " << delta1 << " (should be ~" << target_delta1 <<
          "), delta2 = " << delta2 << " (should be ~" << target_delta2 << ").");
    }
  }

  m_cycles_per_ns = mma_delta.avg();
  Dout(dc::notice, "CPU clock: " << std::fixed << std::setprecision(9) << m_cycles_per_ns << " GHz.");
}

int Benchmark::get_minimum_of(int number_of_runs, std::function<void()> test)
{
  int cycles = std::numeric_limits<int>::max();
  m_stopwatch1->prefetch();
  for (int i = 0; i < number_of_runs; ++i)
  {
    m_stopwatch1->start();
    test();
    m_stopwatch1->stop();
    int ncycles = m_stopwatch1->diff_cycles();
    if (ncycles < cycles)
      cycles = ncycles;
  }
  return cycles;
}

} // namespace benchmark

std::atomic_int s_atomic;

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  using namespace benchmark;

  unsigned int cpu_nr = 0;
  //Benchmark bm(cpu_nr, 0);
  // All CPUs give the same numbers.
  //Benchmark bm(cpu_nr, 141, 222, 3.61205905);         // Without optimization.
  Benchmark bm(cpu_nr, 134, 219, 3.61205905);         // This is with -O3.

  Plot plot("Clocks per assignment in a loop.", "Loopsize", "Number of clocks");
  for (int rm = 1; rm < 90; ++rm)
  {
    std::vector<int> dist(100000, 0);
    int c1 = 0;
    int c2 = 0;
    int m1 = 0;
    int m2 = 0;
    int m3 = 0;

    while (true)
    {
      int cycles = bm.get_minimum_of(1000, [rm](){ for (int r = 0; r < rm; ++r) s_atomic.fetch_add(1); });
      if (cycles < dist.size())
      {
        ++dist[cycles];
        if (dist[cycles] > m1)
        {
          m1 = dist[cycles];
          c1 = cycles;
          if (m2 > 10)
          {
            // Stop if we are 99.9% sure that c1 is the most frequent occurring number of cycles.
            double test_statistic = 1.0 * (m1 - m2) * (m1 - m2) / (m1 + m2);
            if (test_statistic > 10.828)        // 10.828 = critical value of the upper-tail of the chi-square distribution
            {                                   // with one degree of freedom and probability less than 0.999.
              plot.add_data_point(rm, c1 - 94, 0, "99.9%");
              break;
            }
            // Also stop when c1 and c2 are just one clock cycle apart and m2 is significantly larger than m3.
            double test_statistic2 = 1.0 * (m2 - m3) * (m2 - m3) / (m2 + m3);
            if (m2 >= 1000 && std::abs(c1 - c2) == 1 && test_statistic < 2.7 && m3 > 10 && test_statistic2 > 10.828)
            {
              // In this case take the lowest of the two values.
              if (c2 < c1)
              {
                std::swap(c1, c2);
                plot.add_data_point(rm, c1 - 94, 0, "m2");
              }
              else
                plot.add_data_point(rm, c1 - 94, 0, "m1");
              break;
            }
            if (m2 % 100 == 0)
            {
              std::cout << "(c1, m1) = (" << c1 << ", " << m1 << "; (c2, m2) = " << c2 << ", " << m2 << "; m3 = " << m3 << "; test_statistic = " << test_statistic << ", test_statistic2 = " << test_statistic2 << std::endl;
            }
          }
        }
        else if (dist[cycles] > m2)
        {
          m2 = dist[cycles];
          c2 = cycles;
        }
        else if (dist[cycles] > m3)
        {
          m3 = dist[cycles];
        }
      }
    }

    double ns = c1 / bm.cycles_per_ns();
    std::cout << "rm = " << rm << ", c1 = " << c1 << " cycles (" << ns << " ns), c2 = " << c2 << " cycles." << std::endl;
  }
  //plot.set_header("smooth freq");
  //plot.show("boxes");
  //plot.add("set xtics 5");
  //plot.add("set mxtics 5");
  plot.add("set tics out");
  plot.set_header("using 1:2");
  //plot.add("unset key");
  plot.show("points");
}
