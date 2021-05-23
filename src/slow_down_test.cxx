#include "sys.h"
#include "helloworld-task/HelloWorld.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "utils/threading/Gate.h"
#include "threadpool/AIThreadPool.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include <chrono>
#include "debug.h"

namespace utils { using namespace threading; }

// Open the gate to terminate application.
utils::Gate gate;

int main()
{
  Debug(debug::init());
  Dout(dc::notice, "Entering main()");

  // Create a AIMemoryPagePool object (must be created before thread_pool).
  [[maybe_unused]] AIMemoryPagePool mpp;

  // Set up the thread pool for the application.
  int const number_of_threads = 8;                      // Use a thread pool of 8 threads.
  int const max_number_of_threads = 16;                 // This can later dynamically be increased to 16 if needed.
  int const queue_capacity = 1;
  int const reserved_threads = 1;                       // Reserve 1 thread for each priority.
  // Create the thread pool.
  AIThreadPool thread_pool(number_of_threads, max_number_of_threads);
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  // And the thread pool queues.
  [[maybe_unused]] AIQueueHandle high_priority_queue   = thread_pool.new_queue(queue_capacity, reserved_threads);
  [[maybe_unused]] AIQueueHandle medium_priority_queue = thread_pool.new_queue(queue_capacity, reserved_threads);
                   AIQueueHandle low_priority_queue    = thread_pool.new_queue(queue_capacity);

  // Main application begin.
  try
  {
    std::atomic_int finished_counter = 0;
    constexpr int number_of_tasks = 30;

    std::array<boost::intrusive_ptr<task::HelloWorld>, number_of_tasks> tasks;

    for (int i = 0; i < number_of_tasks; ++i)
    {
      tasks[i] = statefultask::create<task::HelloWorld>();
      tasks[i]->initialize(42 + i);
    }


    for (int i = 0; i < number_of_tasks; ++i)
    {
      tasks[i]->run(low_priority_queue, [&, i](bool CWDEBUG_ONLY(success)){
          Dout(dc::notice, "Inside the call-back of task " << i << " (" << (success ? "success" : "failure") << ").");
          if (finished_counter++ == number_of_tasks - 1)
            gate.open();
      });
    }

    // Wait till both callbacks have been called.
    gate.wait();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error << " [caught in slow_down_test.cxx].");
  }

  Dout(dc::notice, "Leaving main()");
}
