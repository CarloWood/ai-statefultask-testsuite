#include "sys.h"
#include "resolver-task/GetAddrInfo.h"
#include "statefultask/AIEngine.h"
#include "evio/EventLoop.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#include <thread>
#include <atomic>

int constexpr queue_capacity = 32;
std::atomic_int test_finished = ATOMIC_VAR_INIT(0);

boost::intrusive_ptr<task::GetAddrInfo> getaddrinfo_task;

void callback(bool success)
{
  if (success)
  {
    std::cout << "Call back is called.\n";
    if (getaddrinfo_task->success())
      std::cout << getaddrinfo_task->get_result() << std::endl;
    else
      std::cout << "There was an error: " << getaddrinfo_task->get_error() << std::endl;
    if (!test_finished)
    {
      getaddrinfo_task->init("irc.undernet.org", 6667);
      getaddrinfo_task->run();
    }
  }
  else
  {
    Dout(dc::notice, "The getaddrinfo_task was aborted!");
  }
  ++test_finished;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  //Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  try
  {
    evio::EventLoop event_loop(handler);
    resolver::Scope resolver_scope(handler, false);

    AIEngine engine("main engine", 2.0);
    getaddrinfo_task = new task::GetAddrInfo(CWDEBUG_ONLY(true));

    getaddrinfo_task->init("www.google.com", "www");
    getaddrinfo_task->run(&engine, &callback);

    // Mainloop.
    Dout(dc::notice, "Starting main loop...");
    while (test_finished < 2)
    {
      engine.mainloop();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Terminate application.
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}
