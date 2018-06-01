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
#include "cwds/gnuplot_tools.h"
#include "utils/macros.h"

int constexpr loopsize = 100;

using clock_type = std::chrono::high_resolution_clock;
using time_point = clock_type::time_point;
using Measurement = eda::FrequencyCounterResult;
template<typename T> using MinAvgMax = eda::MinAvgMax<T>;
template<typename T, int nk> using FrequencyCounter = eda::FrequencyCounter<T, nk>;
using Plot = eda::Plot;

namespace benchmark {

enum show_nt
{
  hide_calibration_graph = 0x1,
  hide_delta_graphs = 0x2,
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
    m_stopwatch1->calibrate_overhead();
  }

  Benchmark(unsigned int cpu_nr, double now_offset = 0.0, int delta = 0, double cycles_per_ns = 0.0, int stopwatch_overhead = 0, int hide_graphs = 0) :
      m_cpu_nr(cpu_nr), m_now_offset(now_offset), m_delta(delta), m_cycles_per_ns(cycles_per_ns)
  {
    m_stopwatch1 = new Stopwatch(m_cpu_nr);
    m_stopwatch2 = new Stopwatch(m_cpu_nr);
    bool calibrated = false;
    if (now_offset == 0.0)
    {
      calibrate_now_offset(!(hide_graphs & hide_calibration_graph));
      calibrated = true;
    }
    if (delta == 0)
    {
      calibrate_delta(!(hide_graphs & hide_delta_graphs));
      calibrated = true;
    }
    if (cycles_per_ns == 0.0)
    {
      calibrate_cycles_per_ns();
      calibrated = true;
    }
    if (stopwatch_overhead == 0)
    {
      m_stopwatch1->calibrate_overhead();
      calibrated = true;
    }
    else
      Stopwatch::s_stopwatch_overhead = stopwatch_overhead;
    if (calibrated)
    {
      DoutFatal(dc::fatal, "Construct Benchmark with Benchmark(" << m_cpu_nr << ", " <<
          m_now_offset << ", " << m_delta << ", " << m_cycles_per_ns << ", " << Stopwatch::s_stopwatch_overhead << ").");
    }
  }

  ~Benchmark()
  {
    delete m_stopwatch2;
    delete m_stopwatch1;
  }

  template<class T>
  Measurement measure(T const functor)
  {
    return m_stopwatch1->measure(functor);
  }
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

} // namespace benchmark

char buf1[200];
std::atomic_int s_atomic;
char buf2[200];
std::mutex m;

void run1()
{
  Debug(NAMESPACE_DEBUG::init_thread());
  Dout(dc::notice, "Start incrementing.");
  int cnt = 0;
  while (++cnt < 1000000000)
    s_atomic.fetch_add(1, std::memory_order_release);
  Dout(dc::notice, "Stop incrementing.");
}

extern int bv;

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
  //Benchmark bm(cpu_nr, ?, ?, 3.61205905);         // Without optimization.
  Benchmark bm(cpu_nr, 112, 217, 3.61205905, 87);   // This is with -O3.

  std::thread t1(run1);

  Plot plot("Clocks per load() in a loop.", "Loopsize", "Number of clocks");
  for (int rm = 1; rm < 100; ++rm)
  {
    auto measurement = bm.measure([rm](){ for (int r = 0; r < rm; ++r) { bv = s_atomic.fetch_add(1); } });
    plot.add_data_point(rm, measurement.m_cycles, 0, measurement.is_t999() ? "99.9%" : measurement.is_tm1() ? "m1" : "m2");
    double ns = measurement.m_cycles / bm.cycles_per_ns();
    std::cout << "rm = " << rm << ", c1 = " << measurement.m_cycles << " cycles (" << ns << " ns)." << std::endl;
  }

  Dout(dc::notice, "s_atomic = " << s_atomic);
  t1.join();

  //plot.set_header("smooth freq");
  //plot.show("boxes");
  //plot.add("set xtics 5");
  //plot.add("set mxtics 5");
  plot.add("set tics out");
  plot.set_header("using 1:2");
  //plot.add("unset key");
  plot.show("points");
}

int bv;
