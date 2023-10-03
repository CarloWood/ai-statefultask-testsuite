#include "sys.h"
#include "debug.h"
#include "statefultask/AITimer.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "threadpool/AIThreadPool.h"
#include <thread>

int constexpr queue_capacity = 32;
bool test_finished = false;

template<threadpool::Timer::time_point::rep count, typename Unit> using Interval = threadpool::Interval<count, Unit>;

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

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  [[maybe_unused]] AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  AIEngine engine("main engine", 2.0);
  boost::intrusive_ptr<AITimer> timer = statefultask::create<AITimer>(CWDEBUG_ONLY(true));

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
