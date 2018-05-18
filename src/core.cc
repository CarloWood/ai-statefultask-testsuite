#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <iostream>
#include <thread>
#include <cassert>
#include "cwds/benchmark.h"

int constexpr bufsize = 64;
int const cpu_nr[2] = { 0, 1 };

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

union Mix
{
  uint64_t data;
  struct X {
    uint64_t m_tsc : 48;        // The least significant bits.
    uint64_t m_index : 16;
  } x;
};

void f(int cpu)
{
  benchmark::Stopwatch stopwatch(cpu_nr[cpu]);
  stopwatch.start();
  Mix u;
  u.x.m_tsc = stopwatch.start_cycles();
  assert((u.x.m_tsc & 0xf00000000000) != 0xf00000000000);

  std::array<std::atomic<uint64_t>, bufsize>& my_tsc_buffer = m_ringbuffers[cpu];
  std::array<std::atomic<uint64_t>, bufsize>& other_tsc_buffer = m_ringbuffers[1 - cpu];
  std::array<int64_t, bufsize>& my_diff_buffer{m_diff[cpu]};

  uint64_t count = 0;
  while (count < 10000000)
  {
    uint64_t tsc_hi, tsc_lo;
    register int64_t r1 = 1;

    // This assembly code is written exclusively for a x86-64.
    __asm__ __volatile__ (
        "lfence\n\t"
        "rdtsc"
        : "=a" (tsc_lo), "=d" (tsc_hi), "+r" (r1));

    r1 = s_atomic.fetch_add(r1);                          // 54 clock cycles.

    __asm__ __volatile__ ("");

    int64_t r2 = r1 - 1;
    int64_t r0 = (tsc_hi << 32) | tsc_lo;
    u.x.m_tsc = (uint64_t)r0;
    u.x.m_index = r1;
    r1 &= bufsize - 1;
    my_tsc_buffer[r1] = u.data;

    barrier();

    r2 &= bufsize - 1;
    Mix u2;
    u2.data = other_tsc_buffer[r2];
    r1 = u2.x.m_tsc;

    __asm__ __volatile__ (
        "rdtscp"
        : "=a" (tsc_lo), "=d" (tsc_hi)
        :: "%rcx");

    int64_t r3 = (tsc_hi << 32) | tsc_lo;
    r3 -= r0;
    if (r3 < 6000)      // 6000 should be roughly 32 * the lowest value of r3. On my box r3 >= 191.
    {
      r0 = u.x.m_tsc;
      r0 -= r1;
      if (r0 < 0)
      {
        std::cout << "CPU " << cpu_nr[cpu] << ": Huh... " << r0 << "; index " << u.x.m_index << " - " << u2.x.m_index << std::endl;
      }
      else if (r0 < my_diff_buffer[r2])
      {
        my_diff_buffer[r2] = r0;
        count = 0;
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
    std::cout << "CPU " << cpu_nr[cpu] << ":";
    for (auto e : m_diff[cpu])
    {
      std::cout << ' ' << e;;
      min[cpu] = std::min(min[cpu], e);
    }
    std::cout << std::endl;
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
