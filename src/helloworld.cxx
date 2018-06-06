#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>

std::atomic_uint number_of_tasks;

class HelloWorld;
class Bumper;

HelloWorld* hello_world;
Bumper* bumper;

class HelloWorld : public AIStatefulTask {
  private:
    bool m_bumped;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum hello_world_state_type {
      HelloWorld_start = direct_base_type::max_state,
      HelloWorld_wait,
      HelloWorld_done,
    };

  public:
    static state_type constexpr max_state = HelloWorld_done + 1;
    HelloWorld();

    void bump() { m_bumped = true; signal(1); }

  protected: // The destructor must be protected.
    ~HelloWorld() override;
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;
};

class Bumper : public AIStatefulTask {
  private:
    bool m_bumped;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum bumper_state_type {
      Bumper_start = direct_base_type::max_state,
      Bumper_wait,
      Bumper_done,
    };

  public:
    static state_type constexpr max_state = Bumper_done + 1;
    Bumper();

    void bump() { m_bumped = true; signal(1); }

  protected: // The destructor must be protected.
    ~Bumper() override;
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;
};

HelloWorld::HelloWorld() : DEBUG_ONLY(AIStatefulTask(true),) m_bumped(false)
{
  DoutEntering(dc::statefultask, "HelloWorld::HelloWorld()");
  ++number_of_tasks;
}

HelloWorld::~HelloWorld()
{
  DoutEntering(dc::statefultask, "HelloWorld::~HelloWorld()");
  --number_of_tasks;
}

char const* HelloWorld::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of hello_world_state_type.
    AI_CASE_RETURN(HelloWorld_start);
    AI_CASE_RETURN(HelloWorld_wait);
    AI_CASE_RETURN(HelloWorld_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

Bumper::Bumper() : DEBUG_ONLY(AIStatefulTask(true),) m_bumped(false)
{
  DoutEntering(dc::statefultask, "Bumper::Bumper()");
  ++number_of_tasks;
}

Bumper::~Bumper()
{
  DoutEntering(dc::statefultask, "Bumper::~Bumper()");
  --number_of_tasks;
}

char const* Bumper::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of bumper_state_type.
    AI_CASE_RETURN(Bumper_start);
    AI_CASE_RETURN(Bumper_wait);
    AI_CASE_RETURN(Bumper_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

void HelloWorld::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case HelloWorld_start:
      set_state(HelloWorld_wait);
      break;
    case HelloWorld_wait:
      if (!m_bumped)
      {
        wait(1);
        break;
      }
      set_state(HelloWorld_done);
      break;
    case HelloWorld_done:
      bumper->bump();
      finish();
      break;
  }
}

void Bumper::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Bumper_start:
      hello_world->bump();
      set_state(Bumper_wait);
      break;
    case Bumper_wait:
      if (!m_bumped)
      {
        wait(1);
        break;
      }
      set_state(Bumper_done);
      break;
    case Bumper_done:
      finish();
      break;
  }
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  // Make sure the debug channel 'statefultask' is turned on.
  Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  static_assert(!std::is_destructible<HelloWorld>::value &&
                std::has_virtual_destructor<HelloWorld>::value,
      "Class must have a protected virtual destuctor.");

  AIEngine engine("main:engine", 1000);
  AIThreadPool thread_pool(2);
  AIQueueHandle high_priority_queue = thread_pool.new_queue(8);

  hello_world = new HelloWorld;
  bumper = new Bumper;

  Dout(dc::statefultask|flush_cf, "Calling hello_world->run()");
  hello_world->run(high_priority_queue);
  Dout(dc::statefultask|flush_cf, "Returned from hello_world->run()");

  Dout(dc::statefultask|flush_cf, "Calling bumper->run()");
  bumper->run(high_priority_queue);
  Dout(dc::statefultask|flush_cf, "Returned from bumper->run()");

  for (int n = 0; n < 100 && number_of_tasks > 0; ++n)
  {
    Dout(dc::statefultask|flush_cf, "Calling engine.mainloop()");
    engine.mainloop();
    Dout(dc::statefultask|flush_cf, "Returned from engine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}
