#include "sys.h"
#include "debug.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "statefultask/AIPackagedTask.h"

// Suppose we need to run this from a task (and wait until it finished).
static int factorial(int n)
{
  DoutEntering(dc::notice, "factorial()");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  int r = 1;
  while (n > 1) r *= n--;
  Dout(dc::notice, "Leaving factorial()");
  return r;
}

static void sayhello()
{
  DoutEntering(dc::notice, "sayhello()");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "Hello!\n";
}

constexpr int capacity = 2;
AIObjectQueue<std::function<void()>> s_task_queue(capacity);

class Task : public AIStatefulTask {
  protected:
    using direct_base_type = AIStatefulTask;            // The base class of this task.
    ~Task() override { }                                // The destructor must be protected.

    // The different states of the task.
    enum task_state_type {
      Task_start = direct_base_type::max_state,
      Task_dispatch_factorial,
      Task_hello,
      Task_dispatch_say_hello,
      Task_done,
    };

    // Override virtual functions.
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;

  public:
    static state_type const max_state = Task_done + 1;  // One beyond the largest state.
    Task() : AIStatefulTask(DEBUG_ONLY(true)),
        m_calculate_factorial(this, 1, &factorial, s_task_queue),
        m_say_hello(this, 2, &sayhello, s_task_queue) { }

  private:
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

void Task::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Task_start:
      m_calculate_factorial(5);
      // Fall through.
    case Task_dispatch_factorial:
    {
      int length = m_calculate_factorial.dispatch();
      if (length == capacity)   // Queue was full.
      {
        set_state(Task_dispatch_factorial);
        yield_frame(1);
        break;
      }
      set_state(Task_hello);
      break;                    // This break is necessary!
    }
    case Task_hello:
      m_say_hello();
      // Fall through.
    case Task_dispatch_say_hello:
    {
      int length = m_say_hello.dispatch();
      if (length == capacity)   // Queue was full.
      {
        set_state(Task_dispatch_say_hello);
        yield_frame(1);
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
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  AIAuxiliaryThread::start();

  boost::intrusive_ptr<Task> task = new Task;

  // Start the test task.
  Dout(dc::statefultask|flush_cf, "Calling task->run()");
  task->run();

  // Mainloop.
  while (!task->finished())
  {
    gMainThreadEngine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!task->aborted())
    Dout(dc::notice, "The task finished successfully.");

  AIAuxiliaryThread::stop();
}
