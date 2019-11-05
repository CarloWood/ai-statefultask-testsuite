#include "sys.h"
#include "debug.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>

constexpr int loopsize = 100000;

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

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < loopsize; ++i)
  {
    asm volatile("");
    flock.unlock();
    flock.lock();
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;
  std::cout << "Average time per lock/unlock pair: " << (1e6 * diff.count() / loopsize) << " µs." << std::endl;

  flock.unlock();
}
