#include "sys.h"
#include "debug.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "statefultask/AIPackagedTask.h"
#include "threadpool/AIThreadPool.h"
#include "threadsafe/Gate.h"

static aithreadsafe::Gate gate;

// Suppose we need to run this from a task (and wait until it finished).
static int factorial(int n)
{
  DoutEntering(dc::notice|flush_cf, "factorial()");
  gate.open();
  int r = 1;
  while (n > 1) r *= n--;
  Dout(dc::notice, "Leaving factorial()");
  return r;
}

static void sayhello()
{
  DoutEntering(dc::notice|flush_cf, "sayhello()");
  gate.wait();
  std::cout << "Hello!\n";
}

class Task : public AIStatefulTask {
  protected:
    using direct_base_type = AIStatefulTask;            // The base class of this task.
    ~Task() override { }                                // The destructor must be protected.

    // The different states of the task.
    enum task_state_type {
      Task_start = direct_base_type::state_end,
      Task_dispatch_factorial,
      Task_hello,
      Task_dispatch_say_hello,
      Task_done,
    };

    // Override virtual functions.
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;

  public:
    static state_type constexpr state_end = Task_done + 1;  // One beyond the largest state.
    Task() : AIStatefulTask(CWDEBUG_ONLY(true)),
        m_task_queue(AIThreadPool::instance().new_queue(capacity)),
        m_calculate_factorial(this, 1, &factorial, m_task_queue),
        m_say_hello(this, 2, &sayhello, m_task_queue) { }

  private:
    static constexpr int capacity = 2;
    AIQueueHandle m_task_queue;
    AIPackagedTask<int(int)> m_calculate_factorial;
    AIPackagedTask<void()> m_say_hello;
};

char const* Task::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of hello_world_state_type.
    AI_CASE_RETURN(Task_start);
    AI_CASE_RETURN(Task_dispatch_factorial);
    AI_CASE_RETURN(Task_hello);
    AI_CASE_RETURN(Task_dispatch_say_hello);
    AI_CASE_RETURN(Task_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
}

AIEngine engine("main engine", 2);

void Task::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Task_start:
      m_calculate_factorial(5);
      [[fallthrough]];
    case Task_dispatch_factorial:
    {
      int length = m_calculate_factorial.dispatch();
      if (length == capacity)   // Queue was full.
      {
        set_state(Task_dispatch_factorial);
        yield_frame(&engine, 1);
        break;
      }
      set_state(Task_hello);
      break;                    // This break is necessary!
    }
    case Task_hello:
      m_say_hello();
      [[fallthrough]];
    case Task_dispatch_say_hello:
    {
      int length = m_say_hello.dispatch();
      if (length == capacity)   // Queue was full.
      {
        set_state(Task_dispatch_say_hello);
        yield_frame(&engine, 1);
        break;
      }
      set_state(Task_done);
      break;                    // This break is necessary!
    }
    case Task_done:
      std::cout << "The result of 5! = " << m_calculate_factorial.get() << std::endl;
      finish();
      break;
  }
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  AIQueueHandle high_priority_queue = thread_pool.new_queue(8);

  boost::intrusive_ptr<Task> task = new Task;

  // Start the test task.
  Dout(dc::statefultask|flush_cf, "Calling task->run()");
  task->run(high_priority_queue);

  // Mainloop.
  while (!task->finished())
  {
    engine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!task->aborted())
  {
    Dout(dc::notice, "The task finished successfully.");
  }
}
