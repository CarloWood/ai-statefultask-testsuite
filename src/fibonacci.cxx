#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>

class Fibonacci;

typedef std::vector<boost::intrusive_ptr<Fibonacci>> flower_type;

class Fibonacci : public AIStatefulTask {
  private:
    flower_type* m_flower;
    int m_index;
    int m_value;
    bool m_smallest_ready;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum fibonacci_state_type {
      Fibonacci_start = direct_base_type::max_state,
      Fibonacci_smallest,
      Fibonacci_largest,
      Fibonacci_math,
      Fibonacci_done,
    };

  public:
    static state_type const max_state = Fibonacci_done + 1;
    Fibonacci() : AIStatefulTask(true), m_flower(NULL), m_index(0), m_value(0), m_smallest_ready(false) { }

    void set_flower(flower_type& flower, int n) { m_flower = &flower; m_index = n; }
    int value() const { return m_value; }

  protected: // The destructor must be protected.
    ~Fibonacci() { }
    void initialize_impl();
    void multiplex_impl(state_type run_state);
    void abort_impl();
    void finish_impl();
    char const* state_str_impl(state_type run_state) const;
};

char const* Fibonacci::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of fibonacci_state_type.
    AI_CASE_RETURN(Fibonacci_start);
    AI_CASE_RETURN(Fibonacci_smallest);
    AI_CASE_RETURN(Fibonacci_largest);
    AI_CASE_RETURN(Fibonacci_math);
    AI_CASE_RETURN(Fibonacci_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

void Fibonacci::initialize_impl()
{
  set_state(Fibonacci_start);
}

void Fibonacci::abort_impl()
{
  DoutEntering(dc::statefultask, "Fibonacci::abort_impl()");
}

void Fibonacci::finish_impl()
{
  DoutEntering(dc::statefultask, "Fibonacci::finish_impl()");
}

void Fibonacci::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Fibonacci_start:
      if (m_index < 2)
      {
	m_value = 1;
	set_state(Fibonacci_done);
	break;
      }
      (*m_flower)[m_index - 1]->run(this, Fibonacci_largest);
      (*m_flower)[m_index - 2]->run(this, Fibonacci_smallest);
      idle();
      break;
    case Fibonacci_smallest:
      m_smallest_ready = true;
      idle();
      break;
    case Fibonacci_largest:
      set_state(Fibonacci_math);
      if (!m_smallest_ready)
	idle();
      break;
    case Fibonacci_math:
      m_value = (*m_flower)[m_index - 1]->value() + (*m_flower)[m_index - 2]->value();
    case Fibonacci_done:
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
  Debug(libcw_do.on());

  static_assert(!std::is_destructible<Fibonacci>::value && std::has_virtual_destructor<Fibonacci>::value, "Class must have a protected virtual destuctor.");

  AIAuxiliaryThread::start();

  flower_type flower;
  int const number = 10;
  for (int n = 0; n < number; ++n)
  {
    flower.push_back(new Fibonacci);
    flower.back()->set_flower(flower, n);
  }

  Dout(dc::statefultask|flush_cf, "Calling fibonacci->run()");
  flower[number - 1]->run();

  for (int n = 0; n < number && flower[number - 1]->value() == 0; ++n)
  {
    Dout(dc::statefultask|flush_cf, "Calling gMainThreadEngine.mainloop()");
    gMainThreadEngine.mainloop();
    Dout(dc::statefultask|flush_cf, "Returned from gMainThreadEngine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  for (int n = 0; n < number; ++n)
    std::cout << flower[n]->value() << std::endl;
}
