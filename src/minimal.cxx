#include "sys.h"
#include "helloworld-task/HelloWorld.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "statefultask/AIEngine.h"
#include "evio/EventLoop.h"
#include "utils/debug_ostream_operators.h"      // Needed to write error to Dout.

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()");

  AIMemoryPagePool mpp;                 // Create before thread_pool.
  AIThreadPool thread_pool;
  AIQueueHandle low_priority_queue = thread_pool.new_queue(8);

  try
  {
    evio::EventLoop event_loop(low_priority_queue);
    AIEngine engine("main engine", 2.0);

    auto task = task::create<task::HelloWorld>();
    task->initialize(42);

    bool test_finished = false;
    task->run(&engine, [&](bool CWDEBUG_ONLY(success)){
        test_finished = true;
        Dout(dc::notice, "Inside the call-back (" << (success ? "success" : "failure") << ").");
    });

    // Add an artificial main loop here.
    while (!test_finished)
    {
      engine.mainloop();
      // Pretend we're doing something else too.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Terminate application.
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()");
}
