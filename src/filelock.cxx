#include "sys.h"
#include "debug.h"
#include "cwds/benchmark.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>

double const cpu_frequency = 3612059050.0;        // In cycles per second.
int const cpu = 0;                                // The CPU to run on.
size_t const loopsize = 1000;                     // We'll be measing the number of clock cylces needed for this many iterations of the test code.
size_t const minimum_of = 3;                      // All but the fastest measurement of this many measurements are thrown away (3 is normally enough).
int const nk = 3;                                 // The number of buckets of FrequencyCounter (with the highest counts) that are averaged over.

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  std::ofstream file("my_lock_file");
  file.close();
  boost::interprocess::file_lock flock("my_lock_file");
  Dout(dc::notice|flush_cf, "Attempting to lock \"my_lock_file\".");
  flock.lock();
  Dout(dc::notice|flush_cf, "Lock obtained; sleeping 100 ms...");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  Dout(dc::notice|flush_cf, "Running benchmark...");

  using namespace benchmark;
  Stopwatch stopwatch(cpu);
  stopwatch.calibrate_overhead(loopsize, minimum_of);
  auto flock_ptr = &flock;
  auto result = stopwatch.measure<nk>(loopsize, [flock_ptr = flock_ptr]() mutable {
      asm volatile ("" : "+r" (flock_ptr));     // See https://stackoverflow.com/a/54245040/1487069 for an explanation and discussion.
      flock_ptr->unlock();
      flock_ptr->lock();
  });

  std::cout << "Average time per lock/unlock pair: " << (result / cpu_frequency * 1e6 / loopsize) << " µs [measured " << result << " clocks]." << std::endl;

  flock.unlock();
}
