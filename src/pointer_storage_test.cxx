#include "sys.h"
#include "threadsafe/PointerStorage.h"
#include <iostream>
#include <thread>
#include <array>
#include <random>
#include <iomanip>
#include "debug.h"

#ifdef __OPTIMIZE__
#define BENCHMARK
#endif

#ifdef BENCHMARK
#include "cwds/benchmark.h"

double const cpu_frequency = 3612059050.0;      // In cycles per second.
int const cpu = 8;
size_t const loopsize = 1000;                   // We'll be measing the number of clock cylces needed for this many iterations of the test code.
size_t const minimum_of = 3;                    // All but the fastest measurement of this many measurements are thrown away (3 is normally enough).
#endif

std::atomic_int counter;

struct A
{
  int n_;
  virtual ~A() { n_ = -1; --counter; }
  A(int n) : n_(n) { ++counter; }
};

struct B : public A
{
  int m_;
  ~B() override { ASSERT(n_ >= 0); }
  B(int n, int m) : A(n), m_(m) { }
};

void f(A* a)
{
  std::cout << "a->n_ = " << a->n_ << std::endl;
}

using PS = aithreadsafe::PointerStorage<A>;
PS ps(2000);

#ifdef CWDEBUG
// Initialization code for new threads.
void init_debug(int thread)
{
  {
    std::ostringstream thread_name;
    thread_name << "thread" << std::dec << std::setfill('0') << std::setw(2) << thread;
    Debug(NAMESPACE_DEBUG::init_thread(thread_name.str()));
  }
}
#endif

void thread(int n, std::vector<PS::index_type>* positions, std::vector<int> const& rn)
{
  Debug(init_debug(n));

#ifdef BENCHMARK
  benchmark::Stopwatch stopwatch(n);          // Declare stopwatch and configure on which CPU it must run.
  size_t msum = 0;
  size_t insert_count = 0;
#endif

  for (int j = 0; j < rn.size(); ++j)
  {
    int ramp = j % 1000;
    int current = positions->size();
    int target = rn[j] + ramp;
    if (target > current)
    {
      for (int k = 0; k < target - current; ++k)
      {
        A* a = new B(n, j);
#ifdef BENCHMARK
        stopwatch.start();
#endif
        int index = ps.insert(a);
#ifdef BENCHMARK
        stopwatch.stop();
        msum += stopwatch.diff_cycles();
        ++insert_count;
#endif
        positions->push_back(index);
      }
    }
    else
    {
      for (int k = 0; k < current - target; ++k)
      {
        auto pos = positions->begin() + (ramp % positions->size());
        int index = *pos;
        A* a = ps.get(index);
        ps.erase(index);
        delete a;
        positions->erase(pos);
      }
    }
    ASSERT(positions->size() == target);
  }
#ifdef BENCHMARK
  Dout(dc::notice, "Average insert time: " << (msum / insert_count) << " clock cycles (" << (msum / insert_count / cpu_frequency * 1e9) << " ns).");
#endif
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

#ifdef BENCHMARK
  benchmark::Stopwatch stopwatch(cpu);          // Declare stopwatch and configure on which CPU it must run.

  // Calibrate Stopwatch overhead.
  stopwatch.calibrate_overhead(loopsize, minimum_of);
#endif

  constexpr int number_of_threads = 16;
  constexpr int number_of_random_numbers_per_thread = 100000;

  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(1, 100);
  std::array<std::vector<int>, number_of_threads> rnd;
  for (int t = 0; t < number_of_threads; ++t)
    for (int r = 0; r < number_of_random_numbers_per_thread; ++r)
      rnd[t].push_back(uni(rng));

  std::array<std::vector<PS::index_type>, number_of_threads> positions;
  std::array<std::thread, number_of_threads> threads;
  for (int t = 0; t < threads.size(); ++t)
    threads[t] = std::thread(thread, t, &positions[t], rnd[t]);

  for (int t = 0; t < threads.size(); ++t)
    threads[t].join();

  ASSERT(!ps.debug_empty());

  int total = 0;
  for (int t = 0; t < threads.size(); ++t)
  {
    Dout(dc::notice, "positions[" << t << "].size() = " << positions[t].size());
    total += positions[t].size();
  }
  ASSERT(total == counter);

  int count = 0;
  ps.for_each([&](A* ptr){ ++count; });
  ASSERT(count == counter);
  Dout(dc::notice, "Success!");
}
