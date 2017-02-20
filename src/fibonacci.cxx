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
      // The first two Fibonacci numbers are 1.
      if (m_index < 2)
      {
	m_value = 1;
	set_state(Fibonacci_done);
	break;
      }
      // Create two new task objects.
      m_largest = new Fibonacci;
      m_largest->set_number(m_index - 1);
      m_smallest = new Fibonacci;
      m_smallest->set_number(m_index - 2);
      // Start subtasks and wait for one or both to be finished.
      m_largest->run(this, Fibonacci_largest);          // If these cause a call to advance_state immediately,
      m_smallest->run(this, Fibonacci_smallest);        // then the call to idle() will be ignored.
      // Wait until one or both subtasks have finished (if they haven't already).
      idle();
      break;
    case Fibonacci_smallest:
      // Getting here means that advance_state(Fibonacci_largest) wasn't called before this state
      // machine started running. If it was called after this state machine started running then
      // a subsequent to idle() will be ignored, as if the call to adavance_state(Fibonacci_largest)
      // was called only at the end of this run (after the call to idle()).
      m_smallest_ready = true;
      if (m_largest_ready)
      {
        // If we get here then Fibonacci_largest has run before, so both subtasks are done.
        set_state(Fibonacci_math);
        break;
      }
      // Wait for adavance_state(Fibonacci_largest) to be called (if it wasn't already in the meantime).
      idle();
      break;
    case Fibonacci_largest:
      // Getting here means that m_largest has finished.
      // Further there are possiblities:
      // 1. Fibonacci_smallest already ran.
      // 2a. Fibonacci_smallest didn't run, but adavance_state(Fibonacci_smallest) was already called.
      //     That means that that adavance_state(Fibonacci_smallest) was ignored.
      // 2b. Fibonacci_smallest didn't run and adavance_state(Fibonacci_smallest) wasn't already called.
      m_largest_ready = true;
      if (!m_smallest_ready)
      {
        // Getting here means that Fibonacci_smallest didn't run yet; but that doesn't
        // mean that m_smallest didn't finish already. It could have finished right before
        // or after this state machine started running because in both cases the
        // adavance_state(Fibonacci_smallest) would have been ignored.
        set_state(Fibonacci_start);
        // If m_smallest did not finished until this point then it's call to
        // adavance_state(Fibonacci_smallest) will no longer be ignored and will
        // cancel the call to idle(), whether or not it happens before or after
        // the call to idle(). So, just calling idle() would be enough for that
        // case.
        //
        // But there is also the possiblity that m_smallest already called
        // adavance_state(Fibonacci_smallest) before we called set_state(Fibonacci_start)
        // in the line above. In order to detect that we have to poll m_smallest
        // and find out that it already finished with certainty in the case that
        // it already called adavance_state(Fibonacci_smallest).
        //
        // adavance_state(Fibonacci_smallest) is called from the base state 'bs_callback',
        // which comes after 'bs_finish' which comes after a flag is set (in the call
        // to finish()) that the task successfully finished. So, 1) if we detect here
        // that that flag is set than we can call value() safely, because m_value
        // is set before the call to finish(), 2) if we detect that the flag is not
        // set then we know for sure that advance_state(Fibonacci_smallest) wasn't
        // called yet before we entered the query for that flag and thus also not
        // directly after we returned from set_state(Fibonacci_start), so that we
        // safely can call idle().
        if (!*m_smallest)
        {
          // m_smallest didn't finish before we set our state to Fibonacci_start,
          // so it is safe to call idle().
          idle();
          break;
        }
        // m_smallest finished successfully, so we can continue to Fibonacci_math.
      }
      set_state(Fibonacci_math);
      /* Fall-through */
    case Fibonacci_math:
      // Both subtasks are done. Calculate our value from the results.
      m_value = m_largest->value() + m_smallest->value();
      set_state(Fibonacci_done);
      /* Fall-through */
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

  static_assert(!std::is_destructible<Fibonacci>::value && std::has_virtual_destructor<Fibonacci>::value, "Class must have a protected virtual destuctor.");

  AIAuxiliaryThread::start();

  int const number = 4;
  Fibonacci* flower = new Fibonacci;
  flower->set_number(number);

  Dout(dc::statefultask|flush_cf, "Calling fibonacci->run()");
  flower->run();

  while (flower->value() == 0)
  {
    //Dout(dc::statefultask|flush_cf, "Calling gMainThreadEngine.mainloop()");
    gMainThreadEngine.mainloop();
    //Dout(dc::statefultask|flush_cf, "Returned from gMainThreadEngine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  std::cout << flower->value() << std::endl;
}
