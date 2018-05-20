#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <iostream>
#include <thread>
#include <cassert>
#include "cwds/benchmark.h"
#include "cwds/gnuplot_tools.h"

int constexpr bufsize = 1024;
int const cpu_nr[2] = { 0, 2 };

std::atomic<uint16_t> s_atomic;
std::array<std::array<std::atomic<uint64_t>, bufsize>, 2> m_ringbuffers;
std::array<std::array<int64_t, bufsize>, 2> m_diff;

void init()
{
  s_atomic.store(10U);
  std::memset(&m_ringbuffers, 0, sizeof(m_ringbuffers));
  std::memset(&m_diff, 0, sizeof(m_diff));
  for (int cpu = 0; cpu <= 1; ++ cpu)
    for (auto&& e : m_diff[cpu]) e = std::numeric_limits<int64_t>::max();
}

#define barrier() asm volatile("": : :"memory")

void f(int cpu)
{
  benchmark::Stopwatch stopwatch(cpu_nr[cpu]);
  stopwatch.start();

  std::array<std::atomic<uint64_t>, bufsize>& my_tsc_buffer = m_ringbuffers[cpu];
  std::array<std::atomic<uint64_t>, bufsize>& other_tsc_buffer = m_ringbuffers[1 - cpu];
  std::array<int64_t, bufsize>& my_diff_buffer{m_diff[cpu]};

  int64_t smallest_delta_with_index_offset = std::numeric_limits<int64_t>::max();

  uint64_t count = 0;
  while (count < 10000000)
  {
    uint64_t tsc1;
    uint64_t tsc2;
    int16_t index;

    // This assembly code is written exclusively for a x86-64.
    __asm__ __volatile__ (
        //"mov $0, %[aindex]\n\t"                         // index = 0;
        //"lock xaddw %[aindex], %[as_atomic]\n\t"        // Warm up cache.
        "mov $1, %[aindex]\n\t"                         // index = 1;
        "lfence\n\t"                                    // Wait till all previous instructions locally finished.
        "rdtsc\n\t"                                     // Read TimeStampCounter into rdx:rax (timestamp1_hi:timestamp1_lo).
        "lock xaddw %[aindex], %[as_atomic]\n\t"        // Atomically, swap = s_atomic; s_atomic += index; index = swap;
        "movq %%rdx, %%rdi\n\t"                         // rdi = timestamp1_hi;
        "movq %%rax, %[tsc1]\n\t"                       // tsc1 = timestamp1_lo;
        "rdtsc\n\t"                                     // Read TimeStampCounter into rdx:rax (timestamp2_hi:timestamp2_lo).
        "salq $32, %[rdx]\n\t"                          // tsc2 = timestamp2_hi << 32;
        "orq %%rax, %[rdx]\n\t"                         // tsc2 |= timestamp2_lo;
        "salq $32, %%rdi\n\t"                           // rdi <<= 32;
        "orq %%rdi, %[tsc1]\n\t"                        // tsc1 |= rdi;
        : [tsc1] "=r" (tsc1),
          [rdx] "=d" (tsc2),
          [aindex] "=&r" (index)
        : [as_atomic] "m" (s_atomic)
        : "%rax", "%rdi");

    int16_t other_index = index - 1;

    index &= bufsize - 1;
    my_tsc_buffer[index] = tsc1;

    barrier();

    other_index &= bufsize - 1;
    uint64_t other_tsc = other_tsc_buffer[other_index];

    uint64_t tsc3_high, tsc3_low;
    __asm__ __volatile__ (
        "rdtscp"
        : "=a" (tsc3_low), "=d" (tsc3_high)
        :: "%rcx");
    int64_t tsc3 = (tsc3_high << 32) | tsc3_low;

    int64_t delta = tsc3 - tsc1;
    if (delta < 6000)      // 6000 should be roughly 32 * the lowest value of delta. On my box delta >= 191.
    {
      delta = tsc1 - other_tsc;
      if (delta < 40000)        // Under 50000 I never see the wrong s_atomic indexes be compared (aka, if delta is less than 50,000
                                // than we can be sure that other_tsc is the timestamp ready by the other core JUST prior to incrementing
                                // s_atomic to the value that WE read into index. And that's the timestamps that we want to compare.
      {
        if (delta < -8000)
        {
          //std::cout << "CPU " << cpu_nr[cpu] << ": Huh... tsc3 - tsc1 = " << delta << "; tsc2 - tsc1 = " << (tsc2 - tsc1) << std::endl;
        }
        else if (delta < my_diff_buffer[other_index])
        {
          my_diff_buffer[other_index] = delta;
          count = 0;
        }
      }
      ++count;
    }
  }
  stopwatch.stop();
  std::cout << "CPU " << cpu_nr[cpu] << " ran for " << stopwatch.diff_cycles() << " cycles." << std::endl;
}

int main()
{
  init();
  std::thread t0([](){ f(0); });
  std::thread t1([](){ f(1); });
  t0.join();
  t1.join();
  int64_t min[2] = { std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max() };
  for (int cpu = 0; cpu <= 1; ++cpu)
  {
    eda::FrequencyCounter<int64_t> frequency_counter;
    std::cout << "CPU " << cpu_nr[cpu] << ":";
    for (int64_t e : m_diff[cpu])
    {
      std::cout << ' ' << e;;
      min[cpu] = std::min(min[cpu], e);
      frequency_counter.add(e / 10);
    }
    std::cout << std::endl;
    eda::PlotHistogram plot("Minimum stable for 100,000 runs, CPU #" + std::to_string(cpu_nr[cpu]), "Minimum (clocks)", "Frequency (count)");
    plot.show(frequency_counter);
  }
  int64_t offset = (min[0] - min[1]) / 2;
  int ahead = 0;
  if (offset < 0)
  {
    offset = -offset;
    ahead = 1;
  }
  std::cout << "The Time Stamp Counter of CPU " << cpu_nr[ahead] << " is " << offset << " ahead of the Time Stamp Counter of CPU " << cpu_nr[1 - ahead] << "." << std::endl;
}
