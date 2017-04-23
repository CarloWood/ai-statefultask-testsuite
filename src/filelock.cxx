#include "sys.h"
#include "debug.h"
#include "microbench/microbench.h"
#include <chrono>
#include <thread>
#include <boost/interprocess/sync/file_lock.hpp>

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  boost::interprocess::file_lock flock("my_lock_file");
  Dout(dc::notice|flush_cf, "Attempting to lock \"my_lock_file\".");
  flock.lock();
  Dout(dc::notice|flush_cf, "Lock obtained; sleeping 100 ms...");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  Dout(dc::notice|flush_cf, "Running benchmark...");

  std::cout << "Average time per lock/unlock pair: " <<
    moodycamel::microbench(
        [&flock]() {            // Function to benchmark.
          flock.unlock();
          flock.lock(); },
        1000,                   // Iterations per test run.
        20,                     // Number of test runs.
        true) * 1000            // ms -> us
    << " µs." << std::endl;

  flock.unlock();
}
