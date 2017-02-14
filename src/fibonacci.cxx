#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>

class Fibonacci : public AIStatefulTask {
  private:
    int m_index;
    int m_value;
    boost::intrusive_ptr<Fibonacci> m_smallest;
    boost::intrusive_ptr<Fibonacci> m_largest;
    bool m_smallest_ready;
    bool m_largest_ready;

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
    Fibonacci() : AIStatefulTask(true), m_index(0), m_value(0), m_smallest_ready(false), m_largest_ready(false) { }

    void set_number(int n) { m_index = n; }
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
      m_largest = new Fibonacci;
      m_largest->set_number(m_index - 1);
      m_smallest = new Fibonacci;
      m_smallest->set_number(m_index - 2);
      idle();
      m_largest->run(this, Fibonacci_largest);
      m_smallest->run(this, Fibonacci_smallest);
      break;
    case Fibonacci_smallest:
      m_smallest_ready = true;
      if (m_largest_ready)
        set_state(Fibonacci_math);
      else
        idle();
      break;
    case Fibonacci_largest:
      m_largest_ready = true;
      if (!m_smallest_ready)
      {
        set_state(Fibonacci_start);
	idle();
        break;
      }
      set_state(Fibonacci_math);
    case Fibonacci_math:
      m_value = m_largest->value() + m_largest->value();
      set_state(Fibonacci_done);
    case Fibonacci_done:
      Dout(dc::notice, "m_index = " << m_index << "; m_value set to " << m_value);
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

  int const number = 3;
  Fibonacci* flower = new Fibonacci;
  flower->set_number(number);

  Dout(dc::statefultask|flush_cf, "Calling fibonacci->run()");
  flower->run();

  for (int n = 0; n < 10000 && flower->value() == 0; ++n)
  {
    //Dout(dc::statefultask|flush_cf, "Calling gMainThreadEngine.mainloop()");
    gMainThreadEngine.mainloop();
    //Dout(dc::statefultask|flush_cf, "Returned from gMainThreadEngine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  std::cout << flower->value() << std::endl;
}
