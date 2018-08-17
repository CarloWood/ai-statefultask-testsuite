#include "sys.h"
#include "debug.h"
#include "resolver-task/AILookupTask.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include <thread>

int constexpr queue_capacity = 32;
bool test_finished = false;

boost::intrusive_ptr<AILookupTask> resolver;

void callback(bool success)
{
  if (success)
  {
    std::cout << "Call back is called.\n";
    std::cout << resolver->get_result() << std::endl;
  }
  else
  {
    Dout(dc::notice, "The resolver was aborted!");
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
  resolver = new AILookupTask(DEBUG_ONLY(true));

  resolver->set_end_point("www.google.com", "www");
  resolver->run(&callback);

  // Mainloop.
  Dout(dc::notice, "Starting main loop...");
  while (!test_finished)
  {
    engine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  Dout(dc::notice, "Leaving main()...");
}
