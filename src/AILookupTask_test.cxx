#include "sys.h"
#include "debug.h"
#include "resolver-task/AILookupTask.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include <thread>
#include <atomic>

int constexpr queue_capacity = 32;
std::atomic_int test_finished = 0;

boost::intrusive_ptr<AILookupTask> lookup_task;

void callback(bool success)
{
  if (success)
  {
    std::cout << "Call back is called.\n";
    if (lookup_task->success())
      std::cout << lookup_task->get_result() << std::endl;
    else
      std::cout << "There was an error: " << lookup_task->get_error() << std::endl;
    if (!test_finished)
    {
      lookup_task->getaddrinfo("irc.undernet.org", 6667);
      lookup_task->run();
    }
  }
  else
  {
    Dout(dc::notice, "The lookup_task was aborted!");
  }
  ++test_finished;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  //Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  AIThreadPool thread_pool;
  AIQueueHandle handler __attribute__ ((unused)) = thread_pool.new_queue(queue_capacity);
  EventLoopThread::instance().init(handler);
  resolver::Resolver::instance().init(false);

  AIEngine engine("main engine", 2.0);
  lookup_task = new AILookupTask(DEBUG_ONLY(true));

  lookup_task->getaddrinfo("www.google.com", "www");
  lookup_task->run(&engine, &callback);

  // Mainloop.
  Dout(dc::notice, "Starting main loop...");
  while (test_finished < 2)
  {
    engine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  EventLoopThread::instance().terminate();

  Dout(dc::notice, "Leaving main()...");
}
