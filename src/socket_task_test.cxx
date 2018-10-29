#include "sys.h"
#include "debug.h"
#include "socket-task/ConnectToEndPoint.h"
#include "statefultask/AIThreadPool.h"
#include "statefultask/AIEngine.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "threadsafe/Condition.h"

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);
  EventLoopThread::instance().init(handler);

  try
  {
    resolver::Resolver::instance().init(handler, false);

    // Allow the main thread to wait until the test finished.
    aithreadsafe::Condition test_finished;

    task::ConnectToEndPoint* task = new task::ConnectToEndPoint(DEBUG_ONLY(true));
    task->set_end_point(AIEndPoint("www.google.com", 80));
    task->run([task, &test_finished](bool success){
          if (!success)
            Dout(dc::warning, "task::ConnectToEndPoint was aborted");
          else
          {
            Dout(dc::notice, "Task with endpoint " << task->get_end_point() << " finished.");
          }
          test_finished.signal();
        });

    // Wait until the test is finished.
    std::lock_guard<AIMutex> lock(test_finished);
    test_finished.wait();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  // Terminate application.
  resolver::Resolver::instance().close();
  EventLoopThread::instance().terminate();

  Dout(dc::notice, "Leaving main()...");
}
