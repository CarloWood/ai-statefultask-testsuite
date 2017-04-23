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

class Task : public AIStatefulTask {
  protected:
    using direct_base_type = AIStatefulTask;            // The base class of this task.
    ~Task() override { }                                // The destructor must be protected.

    // The different states of the task.
    enum task_state_type {
      Task_start = direct_base_type::max_state,
      Task_hello,
      Task_done,
    };

    // Override virtual functions.
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;

  public:
    static state_type const max_state = Task_done + 1;  // One beyond the largest state.
    Task() : AIStatefulTask(DEBUG_ONLY(true)), m_calculate_factorial(this, 1, &factorial), m_say_hello(this, 2, &sayhello) { }

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
    AI_CASE_RETURN(Task_hello);
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
      set_state(Task_hello);
      break;
    case Task_hello:
      m_say_hello();
      set_state(Task_done);
      break;
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
