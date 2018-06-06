#include "sys.h"
#include "debug.h"
#include "statefultask/AITimer.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include <thread>

int constexpr queue_capacity = 32;
bool test_finished = false;

void callback(bool success)
{
  if (success)
    Dout(dc::notice, "The timer timed out.");
  else
    Dout(dc::notice, "The timer was aborted!");
  test_finished = true;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  AIThreadPool thread_pool;
  AIQueueHandle handler __attribute__ ((unused)) = thread_pool.new_queue(queue_capacity);

  AIEngine engine("main engine", 2.0);
  AITimer* timer = new AITimer(true);

  timer->set_interval(5.5);
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
