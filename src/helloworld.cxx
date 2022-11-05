#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "threadpool/AIThreadPool.h"
#include <iostream>
#include <chrono>
#include <atomic>

std::atomic_uint number_of_tasks = ATOMIC_VAR_INIT(0);

class HelloWorld;
class Bumper;

HelloWorld* hello_world;
Bumper* bumper;

// Create a task called HelloWorld with three states.
class HelloWorld : public AIStatefulTask
{
  private:
    bool m_bumped;

  protected:
    using direct_base_type = AIStatefulTask;            // The base class of this task.

    // The different states of the task.
    enum hello_world_state_type {
      HelloWorld_start = direct_base_type::state_end,   // Mandatory first value.
      HelloWorld_wait,
      HelloWorld_done,
    };

  public:
    static state_type constexpr state_end = HelloWorld_done + 1;        // Must be one passed the last state.
    HelloWorld();

    // Raise signal '1' when this function is called.
    void bump() { m_bumped = true; signal(1); }

  protected: // The destructor must be protected.
    // Override virtual functions of the base class.
    ~HelloWorld() override;
    char const* task_name_impl() const override { return "HelloWorld"; }
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;
};

// Create a task called Bumper with three states.
class Bumper : public AIStatefulTask
{
  private:
    bool m_bumped;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum bumper_state_type {
      Bumper_start = direct_base_type::state_end,                       // Mandatory first value.
      Bumper_wait,
      Bumper_done,
    };

  public:
    static state_type constexpr state_end = Bumper_done + 1;            // Must be one passed the last state.
    Bumper();

    // Raise signal '1' when this function is called.
    void bump() { m_bumped = true; signal(1); }

  protected: // The destructor must be protected.
    // Override virtual functions of the base class.
    ~Bumper() override;
    char const* task_name_impl() const override { return "Bumper"; }
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;
};

// Pass true to the base class to turn on debug output for this task.
HelloWorld::HelloWorld() : CWDEBUG_ONLY(AIStatefulTask(true),) m_bumped(false)
{
  DoutEntering(dc::statefultask, "HelloWorld::HelloWorld()");
  ++number_of_tasks;
}

HelloWorld::~HelloWorld()
{
  DoutEntering(dc::statefultask, "HelloWorld::~HelloWorld()");
  --number_of_tasks;
}

// Allow human readable states.
char const* HelloWorld::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of hello_world_state_type.
    AI_CASE_RETURN(HelloWorld_start);
    AI_CASE_RETURN(HelloWorld_wait);
    AI_CASE_RETURN(HelloWorld_done);
  }
  return direct_base_type::state_str_impl(run_state);
};

// Pass true to the base class to turn on debug output for this task.
Bumper::Bumper() : CWDEBUG_ONLY(AIStatefulTask(true),) m_bumped(false)
{
  DoutEntering(dc::statefultask, "Bumper::Bumper()");
  ++number_of_tasks;
}

Bumper::~Bumper()
{
  DoutEntering(dc::statefultask, "Bumper::~Bumper()");
  --number_of_tasks;
}

// Allow human readable states.
char const* Bumper::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of bumper_state_type.
    AI_CASE_RETURN(Bumper_start);
    AI_CASE_RETURN(Bumper_wait);
    AI_CASE_RETURN(Bumper_done);
  }
  return direct_base_type::state_str_impl(run_state);
};

// The HelloWorld task waits until it gets 'bumped'.
// It then bumps bumper and finishes.
void HelloWorld::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case HelloWorld_start:              // First state is the entry point.
      set_state(HelloWorld_wait);       // Continue in state HelloWorld_wait.
      break;
    case HelloWorld_wait:
      if (!m_bumped)
      {
        wait(1);                        // If not bumped yet, wait for signal '1'.
        break;
      }
      set_state(HelloWorld_done);       // If bumped continue with state HelloWorld_done.
      break;
    case HelloWorld_done:
      bumper->bump();                   // Bump bumper,
      finish();                         // and finish.
      break;
  }
}

// The Bumper task bumps hello_world and then
// waits until it gets bumped itself, after which
// it finishes.
void Bumper::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Bumper_start:                  // First state is the entry point.
      hello_world->bump();              // Bump hello_world.
      set_state(Bumper_wait);           // Continue in the state Bumper_wait.
      break;
    case Bumper_wait:
      if (!m_bumped)
      {
        wait(1);                        // If not bumped yet, wait for signal '1'.
        break;
      }
      set_state(Bumper_done);           // If bumped continue with state Bumper_done.
      break;
    case Bumper_done:
      finish();                         // Finish.
      break;
  }
}

int main()
{
  // Initialize debugging code. This must be the first line in main().
  Debug(NAMESPACE_DEBUG::init());
  // Make sure the debug channel 'statefultask' is turned on.
  Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  // Just a test to make sure that the destructor is protected.
  // That avoids that one accidently creates a Task on the stack.
  // Tasks must be created using new.
  static_assert(!std::is_destructible<HelloWorld>::value &&
                std::has_virtual_destructor<HelloWorld>::value,
      "Class HelloWorld must have a protected virtual destuctor.");
  static_assert(!std::is_destructible<Bumper>::value &&
                std::has_virtual_destructor<Bumper>::value,
      "Class Bumper must have a protected virtual destuctor.");

  // Initialize the default memory page pool.
  AIMemoryPagePool mpp;

  // Create a thread pool. Immediately afterwards call new_queue on it at least once!
  AIThreadPool thread_pool;
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  AIQueueHandle high_priority_queue = thread_pool.new_queue(8);

  // Create an engine. This application isn't really using it,
  // because everything runs in the thread pool.
  // Use (any) max_duration (here 10 ms) to cause engine.mainloop()
  // to return (immediately, because it has nothing to do).
  AIEngine engine("main:engine", 10);

  // Create our two custom tasks. This uses normal pointers (as opposed
  // to boost::intrusive_ptr<AIStatefulTask>) which means that they
  // are destructed immediately after finishing. That can be the case
  // immediately after returning from run(), so do not use hello_world
  // or bumper anymore after calling run()!
  hello_world = new HelloWorld;
  bumper = new Bumper;

  // Run the hello_world task in the thread pool through the high_priority_queue.
  Dout(dc::statefultask|flush_cf, "Calling hello_world->run()");
  hello_world->run(high_priority_queue);
  Dout(dc::statefultask|flush_cf, "Returned from hello_world->run()");

  // Run the bumper task in the thread pool through the high_priority_queue.
  Dout(dc::statefultask|flush_cf, "Calling bumper->run()");
  bumper->run(high_priority_queue);
  Dout(dc::statefultask|flush_cf, "Returned from bumper->run()");

  // Wait till both tasks are finished. This loop theoretically
  // allows use to use engine to also run a task in.
  for (int n = 0; n < 100 && number_of_tasks > 0; ++n)
  {
    Dout(dc::statefultask|flush_cf, "Calling engine.mainloop()");
    engine.mainloop();
    Dout(dc::statefultask|flush_cf, "Returned from engine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}
