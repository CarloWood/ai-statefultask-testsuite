#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIThreadPool.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>

AIQueueHandle high_priority_queue;

class Fibonacci : public AIStatefulTask {
  private:
    int m_index;
    int m_value;
    boost::intrusive_ptr<Fibonacci> m_smallest;
    boost::intrusive_ptr<Fibonacci> m_largest;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum fibonacci_state_type {
      Fibonacci_start = direct_base_type::max_state,
      Fibonacci_wait,
      Fibonacci_math,
      Fibonacci_done,
    };

  public:
    static state_type constexpr max_state = Fibonacci_done + 1;
    Fibonacci() : DEBUG_ONLY(AIStatefulTask(true),) m_index(0), m_value(0) { }

    void set_number(int n) { m_index = n; }
    int value() const { return m_value; }

  protected: // The destructor must be protected.
    ~Fibonacci() override { }
    char const* state_str_impl(state_type run_state) const override;
    void multiplex_impl(state_type run_state) override;
};

char const* Fibonacci::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of fibonacci_state_type.
    AI_CASE_RETURN(Fibonacci_start);
    AI_CASE_RETURN(Fibonacci_wait);
    AI_CASE_RETURN(Fibonacci_math);
    AI_CASE_RETURN(Fibonacci_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
};

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
      m_largest->run(high_priority_queue, this, 1);          // Let both sub tasks signal the same bit.
      m_smallest->run(high_priority_queue, this, 1);
      // Wait until one or both subtasks have finished (if they haven't already).
      set_state(Fibonacci_wait);
      /*fall-through*/
    case Fibonacci_wait:
      if (!(m_largest->finished() && m_smallest->finished()))
      {
        wait(1);
        break;
      }
      // If we get here then both subtasks are done.
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
  Debug(if (!dc::statefultask.is_on()) dc::statefultask.on());

  static_assert(!std::is_destructible<Fibonacci>::value && std::has_virtual_destructor<Fibonacci>::value, "Class must have a protected virtual destuctor.");

  AIEngine engine("main:engine");
  AIThreadPool thread_pool;
  high_priority_queue = thread_pool.new_queue(100);

  int const number = 10;
  boost::intrusive_ptr<Fibonacci> flower = new Fibonacci;
  flower->set_number(number);

  Dout(dc::statefultask|flush_cf, "Calling fibonacci->run()");
  flower->run(&engine);
  Dout(dc::statefultask|flush_cf, "Returned from fibonacci->run()");

  while (flower->value() == 0)
  {
    Dout(dc::statefultask|flush_cf, "Calling engine.mainloop()");
    engine.mainloop();
    Dout(dc::statefultask|flush_cf, "Returned from engine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  std::cout << flower->value() << std::endl;
}
