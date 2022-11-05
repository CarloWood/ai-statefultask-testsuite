#include "sys.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "threadpool/AIThreadPool.h"
#include "debug.h"

class Test : public AIStatefulTask
{
 private:
  int m_test;

 public:
  static constexpr int number_of_tests = 66;

 protected:
  /// The base class of this task.
  using direct_base_type = AIStatefulTask;

  /// The different states of the stateful task.
  enum my_task_state_type {
    Test_start = direct_base_type::state_end,
    Test_last = Test_start + number_of_tests - 1
  };

 public:
  /// One beyond the largest state of this task.
  static state_type constexpr state_end = Test_last + 1;

 public:
  Test() : AIStatefulTask(CWDEBUG_ONLY(true)), m_test(0) { }

  void set_test(int test) { m_test = test; }
  void do_test(int test);

  void signal(condition_type condition, bool expected)
  {
    bool res = AIStatefulTask::signal(condition);
    ASSERT(res == expected);
  }

 protected:
  char const* task_name_impl() const override { return "Test"; }
  char const* state_str_impl(state_type run_state) const override;
  void multiplex_impl(state_type run_state) override;
  void initialize_impl() override { AIStatefulTask::set_state(Test_start + m_test); }

  void assert_IDLE() const;
  void assert_RUNNING();
  void assert_SKIP_WAIT();
};

char const* Test::state_str_impl(state_type run_state) const
{
  // This test is single threaded, so we can use a static buffer.
  static std::thread::id s_id;
  assert(aithreadid::is_single_threaded(s_id));
  static char buf[16];
  if (run_state >= Test_start && run_state < Test_start + number_of_tests)
  {
    snprintf(buf, sizeof(buf), "Test_%d", run_state - Test_start);
    return buf;
  }

  return direct_base_type::state_str_impl(run_state);
}

void Test::multiplex_impl(state_type run_state)
{
  ASSERT(Test_start <= run_state && run_state <= Test_last);
  // Reset was just called.
  ASSERT(!waiting());
  Debug(dc::statefultask.on());
  do_test(run_state - Test_start);
  Debug(dc::statefultask.off());
  abort();
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Debug(if (dc::statefultask.is_on()) dc::statefultask.off());
  Debug(if (!dc::notice.is_on()) dc::notice.on());

  AIEngine engine("main");

  for (int test = 0; test < Test::number_of_tests; ++test)
  {
    auto task = statefultask::create<Test>();
    task->set_test(test);
    task->run(&engine);
    engine.mainloop();
  }

  Debug(dc::notice.off());
}

// For each condition bit, there are essentially three states (here named IDLE, RUNNING and SKIP_WAIT).
// SKIP_WAIT is also running (not waiting()). IDLE means that waiting() will return true (all of these states
// are with base state bs_multiplex). The notation signal(xyz) means that signal() returns xyz.
//
//                           --signal(true)-->    --signal(false)-->     --signal(false)--.
//                       IDLE               RUNNING                SKIP_WAIT              |
//    assert <---wait---     <------wait------    <------wait-------     <----------------'
//
// Upon entering do_test(), the state is always RUNNING.

void Test::assert_IDLE() const
{
  // The IDLE state is the only state that is waiting.
  ASSERT(waiting());    // IDLE
}

void Test::assert_RUNNING()
{
  ASSERT(!waiting());           // This would fail if we're waiting for a condition other than 1. Therefore a wait(2) without a signal(2) is not allowed to proceed.
  wait(1);
  ASSERT(waiting());
}

void Test::assert_SKIP_WAIT()
{
  ASSERT(!waiting());           // This would fail if we're waiting for a condition other than 1. Therefore a wait(2) without a signal(2) is not allowed to proceed.
  wait(1);
  ASSERT(!waiting());
}

void Test::do_test(int test)
{
  DoutEntering(dc::notice, "Test::do_test(" << test << ")");
  switch (test)
  {
    // No signal(2).
    case 0:
    {
      wait(1);                  // IDLE <-- RUNNING
      assert_IDLE();
      break;
    }
    case 1:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 3:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 4:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      assert_RUNNING();
      break;
    }

    // Just signal(2).
    case 0+5:
    {
      signal(2, false);
      wait(1);                  // IDLE <-- RUNNING
      assert_IDLE();
      break;
    }
    case 0+6:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(2, false);
      assert_IDLE();
      break;
    }
    case 1+6:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 1+7:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      assert_SKIP_WAIT();
      break;
    }
    case 2+7:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+8:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+9:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      signal(2, false);
      assert_SKIP_WAIT();
      break;
    }
    case 3+9:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+10:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+11:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      signal(2, false);
      assert_RUNNING();
      break;
    }
    case 4+11:
    {
      signal(2, false);
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      assert_RUNNING();
      break;
    }
    case 4+12:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(2, false);
      signal(1, true);          // IDLE --> RUNNING
      assert_RUNNING();
      break;
    }
    case 4+13:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      signal(2, false);
      assert_RUNNING();
      break;
    }

    // signal(2) and wait(2).
    case 0+5+13:
    {
      wait(2);
      signal(2, true);
      wait(1);                  // IDLE <-- RUNNING
      assert_IDLE();
      break;
    }
    case 0+5+14:
    {
      signal(2, false);
      wait(2);
      wait(1);                  // IDLE <-- RUNNING
      assert_IDLE();
      break;
    }
    case 0+5+15:
    {
      signal(2, false);
      wait(3);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_RUNNING();
      break;
    }
    case 0+6+15:
    {
      wait(3);                  // IDLE <-- RUNNING
      signal(2, true);
      assert_RUNNING();
      break;
    }
    case 0+6+16:
    {
      break;
    }
    case 0+6+17:
    {
      break;
    }
    case 1+6+17:
    {
      wait(2);
      signal(2, true);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 1+6+18:
    {
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 1+6+19:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_SKIP_WAIT();       // RUNNING would have been OK too; after the wait(2) we leave multiplex_impl and then run again. The task then should not use 1 because
                                // that was used before running. Nevertheless, maybe it has some use this way.
      break;
    }
    case 1+7+19:
    {
      wait(2);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, true);
      assert_SKIP_WAIT();       // RUNNING would have been OK too; the signal(2) woke up the task which now should not use wait(1), because 1 was used before waking up.
                                // Nevertheless, maybe it has some use this way.
      break;
    }
    case 1+7+20:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);
      signal(2, true);
      assert_SKIP_WAIT();
      break;
    }
    case 1+7+21:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_SKIP_WAIT();
      break;
    }
    case 2+7+21:
    {
      wait(2);
      signal(2, true);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+7+22:
    {
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+7+23:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+7+24:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_SKIP_WAIT();
      break;
    }
    case 2+8+24:
    {
      wait(2);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, true);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+8+25:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);
      signal(2, true);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+8+26:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      signal(1, false);         // RUNNING -> SKIP_WAIT
      assert_SKIP_WAIT();
      break;
    }
    case 2+8+27:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_SKIP_WAIT();
      break;
    }
    case 2+9+27:
    {
      wait(2);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      signal(2, true);
      assert_SKIP_WAIT();
      break;
    }
    case 2+9+28:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      signal(2, true);
      assert_SKIP_WAIT();
      break;
    }
    case 2+9+29:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      wait(2);
      signal(2, true);
      assert_SKIP_WAIT();
      break;
    }
    case 2+9+30:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(1, false);         //             SKIP_WAIT -> SKIP_WAIT
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      assert_SKIP_WAIT();
      break;
    }
    case 3+9+30:
    {
      wait(2);
      signal(2, true);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+9+31:
    {
      signal(2, false);
      wait(2);                  // Because the wait(2) combined with signal(2), we are now RUNNING again.
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+9+32:
    {
      signal(2, false);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(3);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+9+33:
    {
      break;
    }
    case 3+10+33:
    {
      wait(2);
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, true);
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+10+34:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(2);
      signal(2, true);
      wait(1);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+10+35:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      signal(2, false);
      wait(3);                  // RUNNING <-- SKIP_WAIT
      assert_RUNNING();
      break;
    }
    case 3+10+36:
    {
      break;
    }
    case 3+11+36:
    {
      break;
    }
    case 3+11+37:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(3);                  // RUNNING <-- SKIP_WAIT
      signal(2, false);
      assert_RUNNING();
      break;
    }
    case 3+11+38:
    {
      signal(1, false);         // RUNNING --> SKIP_WAIT
      wait(1);                  // RUNNING <-- SKIP_WAIT
      signal(2, false);
      wait(2);
      assert_RUNNING();
      break;
    }
    case 3+11+39:
    {
      break;
    }
    case 4+11+39:
    {
      wait(2);
      signal(2, true);
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      assert_RUNNING();
      break;
    }
    case 4+11+40:
    {
      signal(2, false);
      wait(3);                  // IDLE <-- RUNNING
      signal(1, false);         // IDLE --> RUNNING
      assert_SKIP_WAIT();       // Should have been RUNNING, see 3+9+32? This is why you can't reuse condition wait(1) after having waited on 3.
      break;
    }
    case 4+11+41:
    {
      signal(2, false);
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      wait(2);
      assert_RUNNING();
      break;
    }
    case 4+11+42:
    {
      break;
    }
    case 4+12+42:
    {
      wait(3);                  // IDLE <-- RUNNING
      signal(2, true);
      signal(1, false);         // IDLE --> RUNNING
      assert_SKIP_WAIT();       // Should have been RUNNING, see 3+9+32? This is why you can't reuse condition wait(1) after having waited on 3.
      break;
    }
    case 4+12+43:
    {
      break;
    }
    case 4+12+44:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(2, false);
      signal(1, true);          // IDLE --> RUNNING
      wait(2);
      assert_RUNNING();
      break;
    }
    case 4+12+45:
    {
      break;
    }
    case 4+13+45:
    {
      wait(3);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      signal(2, false);
      assert_RUNNING();
      break;
    }
    case 4+13+46:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      wait(2);
      signal(2, true);
      assert_RUNNING();
      break;
    }
    case 4+13+47:
    {
      wait(1);                  // IDLE <-- RUNNING
      signal(1, true);          // IDLE --> RUNNING
      signal(2, false);
      wait(2);
      assert_RUNNING();
      break;
    }
    case 4+13+48:
    {
      break;
    }

    default:
      ASSERT(false);
  }
}
