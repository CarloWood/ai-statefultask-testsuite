#include "sys.h"
#include "debug.h"
#include "statefultask/AITimer.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include <thread>

int constexpr queue_capacity = 32;
bool test_finished = false;

template<statefultask::Timer::time_point::rep count, typename Unit> using Interval = statefultask::Interval<count, Unit>;

std::chrono::time_point<std::chrono::system_clock> start_time;

void callback(bool success)
{
  if (success)
  {
    std::chrono::time_point<std::chrono::system_clock> end_time = std::chrono::system_clock::now();
    std::cout << "Timed out after " << (std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 100) / 10.0 << " milliseconds." << std::endl;
  }
  else
  {
    Dout(dc::notice, "The timer was aborted!");
  }
  test_finished = true;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  //Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  AIThreadPool thread_pool;
  AIQueueHandle handler __attribute__ ((unused)) = thread_pool.new_queue(queue_capacity);

  AIEngine engine("main engine", 2.0);
  AITimer* timer = new AITimer(DEBUG_ONLY(true));

  timer->set_interval(Interval<2, std::chrono::milliseconds>());

  start_time = std::chrono::system_clock::now();
  timer->run(&callback);

  // Mainloop.
  Dout(dc::notice, "Starting main loop...");
  while (!test_finished)
  {
    engine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  Dout(dc::notice, "Leaving main()...");
}
