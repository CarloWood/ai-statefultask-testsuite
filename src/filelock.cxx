#include "sys.h"
#include "debug.h"
#include "cwds/benchmark.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>

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
  Stopwatch stopwatch;
  stopwatch.calibrate_overhead();
  auto result = stopwatch.measure([&](){ flock.unlock(); flock.lock(); });

  std::cout << "Average time per lock/unlock pair: " << (result / 3612.05905) << " µs." << std::endl;

  flock.unlock();
}
