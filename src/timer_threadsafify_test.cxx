#include "sys.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "utils/threading/Gate.h"
#include "threadpool/AIThreadPool.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include <chrono>
#include "debug.h"

#ifdef DEBUG_SPECIFY_NOW

namespace utils { using namespace threading; }

using Timer = threadpool::Timer;
template<Timer::time_point::rep count, typename Unit> using Interval = threadpool::Interval<count, Unit>;

// Open the gate to terminate application.
utils::Gate gate;
std::atomic_int callbacks = 0;

int main()
{
  Debug(debug::init());
  Dout(dc::notice, "Entering main()");

  // Make a table with time intervals.
  constexpr int number_of_intervals = 30;
  constexpr int base = 50;
  std::array<Timer::Interval, number_of_intervals> intervals = {
    Interval<2 * base, std::chrono::microseconds>(),
    Interval<3 * base, std::chrono::microseconds>(),
    Interval<5 * base, std::chrono::microseconds>(),
    Interval<7 * base, std::chrono::microseconds>(),
    Interval<11 * base, std::chrono::microseconds>(),
    Interval<13 * base, std::chrono::microseconds>(),
    Interval<17 * base, std::chrono::microseconds>(),
    Interval<19 * base, std::chrono::microseconds>(),
    Interval<23 * base, std::chrono::microseconds>(),
    Interval<29 * base, std::chrono::microseconds>(),
    Interval<31 * base, std::chrono::microseconds>(),
    Interval<37 * base, std::chrono::microseconds>(),
    Interval<41 * base, std::chrono::microseconds>(),
    Interval<43 * base, std::chrono::microseconds>(),
    Interval<47 * base, std::chrono::microseconds>(),
    Interval<53 * base, std::chrono::microseconds>(),
    Interval<59 * base, std::chrono::microseconds>(),
    Interval<61 * base, std::chrono::microseconds>(),
    Interval<67 * base, std::chrono::microseconds>(),
    Interval<71 * base, std::chrono::microseconds>(),
    Interval<73 * base, std::chrono::microseconds>(),
    Interval<79 * base, std::chrono::microseconds>(),
    Interval<83 * base, std::chrono::microseconds>(),
    Interval<89 * base, std::chrono::microseconds>(),
    Interval<97 * base, std::chrono::microseconds>(),
    Interval<101 * base, std::chrono::microseconds>(),
    Interval<103 * base, std::chrono::microseconds>(),
    Interval<107 * base, std::chrono::microseconds>(),
    Interval<109 * base, std::chrono::microseconds>(),
    Interval<113 * base, std::chrono::microseconds>()
  };

  // Create a AIMemoryPagePool object (must be created before thread_pool).
  [[maybe_unused]] AIMemoryPagePool mpp;

  // Set up the thread pool for the application.
  int const number_of_threads = 30;                     // Use a thread pool of 30 threads.
  int const max_number_of_threads = 30;                 // This can later dynamically be increased to 16 if needed.
  int const queue_capacity = 1000;
  int const reserved_threads = 1;                       // Reserve 1 thread for each priority.
  // Create the thread pool.
  AIThreadPool thread_pool(number_of_threads, max_number_of_threads);
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  // And the thread pool queues.
  [[maybe_unused]] AIQueueHandle high_priority_queue   = thread_pool.new_queue(queue_capacity, reserved_threads);
  [[maybe_unused]] AIQueueHandle medium_priority_queue = thread_pool.new_queue(queue_capacity, reserved_threads);
                   AIQueueHandle low_priority_queue    = thread_pool.new_queue(queue_capacity);

  // Set 'now' to 1 second into the future, so that we have time to create and start all the timers.
  Timer::time_point now = Timer::clock_type::now() + std::chrono::seconds(1);

  // Main application begin.
  try
  {
    constexpr int number_of_timers_per_interval = 1000;
    constexpr int number_of_timers = number_of_intervals * number_of_timers_per_interval;
    Timer timer[number_of_timers];
    std::array<std::atomic_int, number_of_timers> been_here;
    for (int t = 0; t < number_of_timers; ++t)
      been_here[t] = 0;

    for (int i = 0; i < intervals.size(); ++i)
    {
      for (int t = number_of_timers_per_interval * i; t < number_of_timers_per_interval * (i + 1); ++t)
      {
        timer[t].start(intervals[i], [&, t](){
          // The expiration function should only be called once!
          ASSERT(been_here[t]++ == 0);
          callbacks++;
          if (callbacks == number_of_timers)
            gate.open();
        }, now);
      }
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(2));

    //timer.stop();

    // Wait till program finished.
    gate.wait();
    for (int t = 0; t < number_of_timers; ++t)
    {
      // The expiration function must have been called.
      ASSERT(been_here[t] == 1);
    }
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error << " [caught in timer_threadsafify_test.cxx].");
  }

  Dout(dc::notice, "Leaving main()");
}

#else // DEBUG_SPECIFY_NOW
int main()
{
  std::cerr << "Define DEBUG_SPECIFY_NOW for this test to work." << std::endl;
}
#endif // DEBUG_SPECIFY_NOW
