#include "sys.h"
#include "resolver-task/GetNameInfo.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "evio/SocketAddress.h"
#include "evio/EventLoop.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#include <iostream>
#include <thread>
#include <atomic>

int constexpr queue_capacity = 32;
std::atomic_int test_finished = ATOMIC_VAR_INIT(0);

boost::intrusive_ptr<task::GetNameInfo> getnameinfo_task;

void callback(bool success)
{
  if (success)
  {
    std::cout << "Call back is called.\n";
    if (getnameinfo_task->success())
      std::cout << "Result: \"" << getnameinfo_task->get_result() << "\"." << std::endl;
    else
      std::cout << "There was an error: " << getnameinfo_task->get_error() << std::endl;
#if 0
    if (!test_finished)
    {
      getaddrinfo_task->init("irc.undernet.org", 6667);
      getaddrinfo_task->run();
    }
#endif
  }
  else
  {
    Dout(dc::notice, "The getnameinfo_task was aborted!");
  }
  ++test_finished;
}

int main(int argc, char* argv[])
{
  Debug(NAMESPACE_DEBUG::init());

  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <IP address>" << std::endl;
    return 1;
  }

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  try
  {
    evio::EventLoop event_loop(handler);
    resolver::Scope resolver_scope(handler, false);

    evio::SocketAddress socket_address(argv[1]);
    std::cout << "socket_address = " << socket_address << std::endl;

    AIEngine engine("main engine", 2.0);
    getnameinfo_task = new task::GetNameInfo(CWDEBUG_ONLY(true));
    getnameinfo_task->init(socket_address);
    getnameinfo_task->run(&engine, &callback);

    // Mainloop.
    Dout(dc::notice, "Starting main loop...");
    while (test_finished < 1)
    {
      engine.mainloop();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Terminate application.
    getnameinfo_task.reset();
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}
