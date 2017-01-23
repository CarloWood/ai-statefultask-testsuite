#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>

std::atomic_uint number_of_tasks;

class HelloWorld;
class Bumper;

HelloWorld*  hello_world;
Bumper* bumper;

class HelloWorld : public AIStatefulTask {
  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum hello_world_state_type {
      HelloWorld_wait = direct_base_type::max_state,
      HelloWorld_done,
    };

  public:
    static state_type const max_state = HelloWorld_done + 1;
    HelloWorld();

  protected: // The destructor must be protected.
    ~HelloWorld();
    void initialize_impl();
    void multiplex_impl(state_type run_state);
    void abort_impl();
    void finish_impl();
    char const* state_str_impl(state_type run_state) const;
};

class Bumper : public AIStatefulTask {
  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum bumper_state_type {
      Bumper_wait = direct_base_type::max_state,
      Bumper_done,
    };

  public:
    static state_type const max_state = Bumper_done + 1;
    Bumper();

    void done() { advance_state(Bumper_done); }

  protected: // The destructor must be protected.
    ~Bumper();
    void initialize_impl();
    void multiplex_impl(state_type run_state);
    void abort_impl();
    void finish_impl();
    char const* state_str_impl(state_type run_state) const;
};

HelloWorld::HelloWorld() DEBUG_ONLY(: AIStatefulTask(true))
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
    AI_CASE_RETURN(HelloWorld_wait);
    AI_CASE_RETURN(HelloWorld_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

void HelloWorld::initialize_impl()
{
  set_state(HelloWorld_wait);
}

void HelloWorld::abort_impl()
{
  DoutEntering(dc::statefultask, "HelloWorld::abort_impl()");
}

void HelloWorld::finish_impl()
{
  DoutEntering(dc::statefultask, "HelloWorld::finish_impl()");
}

Bumper::Bumper() DEBUG_ONLY(: AIStatefulTask(true))
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
    AI_CASE_RETURN(Bumper_wait);
    AI_CASE_RETURN(Bumper_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

void Bumper::initialize_impl()
{
  set_state(Bumper_wait);
}

void Bumper::abort_impl()
{
  DoutEntering(dc::statefultask, "Bumper::abort_impl()");
}

void Bumper::finish_impl()
{
  DoutEntering(dc::statefultask, "Bumper::finish_impl()");
}

void HelloWorld::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case HelloWorld_wait:
      set_state(HelloWorld_done);       // set_state() voids the last call to idle(), so it must be called before we call idle().
      idle();                           // Wait until bumper calls cont() on us.
      break;
    case HelloWorld_done:
      bumper->done();
      finish();
      break;
  }
}

void Bumper::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Bumper_wait:
      set_state(Bumper_done);           // set_state() voids the last call to idle(), so it must be called before we call idle().
      hello_world->cont();              // This will immediately run hello_world, and thus call bumper->cont(), which will void the last call to idle().
      idle();                           // Wait until hello_world calls cont() on us.
      break;
    case Bumper_done:
      finish();
      break;
  }
}

int main()
{
#ifdef DEBUG_GOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());
  Debug(libcw_do.on());

  static_assert(!std::is_destructible<HelloWorld>::value && std::has_virtual_destructor<HelloWorld>::value, "Class must have a protected virtual destuctor.");

  AIAuxiliaryThread::start();

  hello_world = new HelloWorld;
  bumper = new Bumper;

  Dout(dc::statefultask|flush_cf, "Calling hello_world->run()");
  hello_world->run();
  Dout(dc::statefultask|flush_cf, "Returned from hello_world->run()");

  Dout(dc::statefultask|flush_cf, "Calling bumper->run()");
  bumper->run();
  Dout(dc::statefultask|flush_cf, "Returned from bumper->run()");

  for (int n = 0; n < 100 && number_of_tasks > 0; ++n)
  {
    Dout(dc::statefultask|flush_cf, "Calling gMainThreadEngine.mainloop()");
    gMainThreadEngine.mainloop();
    Dout(dc::statefultask|flush_cf, "Returned from gMainThreadEngine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}
