#include "sys.h"
#include "statefultask/DefaultMemoryPagePool.h" // AIMemoryPagePool.
#include "statefultask/AIEngine.h"              // AIEngine, AIThreadPool, AIQueueHandle.
#include "evio/EventLoop.h"                     // evio::EventLoop (and AIAlert).
#include "utils/debug_ostream_operators.h"      // Needed to write AIAlert::Error to Dout.
#include "utils/threading/Gate.h"

#include "statefultask/AIStatefulTask.h"

namespace task {

class ThreadPoolYieldTest : public AIStatefulTask
{
 public:
  int m_yield_code;
  threadpool::Timer m_timer;

  Handler handler[4] = {
    Handler::immediate,
    Handler::idle,
    Handler::idle,
    Handler::idle
  };

  void initialize_handler_array(AIEngine* engine_ptr, AIQueueHandle low_priority_queue, AIQueueHandle high_priority_queue)
  {
    handler[1] = engine_ptr;
    handler[2] = low_priority_queue;
    handler[3] = high_priority_queue;
  }

  enum class H {
    immediate,
    engine,
    low,
    high
  };

  void set_yield_code(int yield_code)
  {
    m_yield_code = yield_code;
  }

  H run_handler() const
  {
    return (H)(m_yield_code & 0x3);
  }

  H start_handler() const
  {
    H code = (H)((m_yield_code >> 2) & 0x3);
    return (code == H::immediate) ? run_handler() : code;
  }

  H print_handler() const
  {
    H code = (H)((m_yield_code >> 4) & 0x3);
    return (code == H::immediate) ? start_handler() : code;
  }

  Handler const& get_handler(H which) const
  {
    return handler[(int)which];
  }

  void start_call_yield(Handler current_handler)
  {
    Dout(dc::notice, "Current handler: " << current_handler);
    H code = (H)((m_yield_code >> 2) & 0x3);
    if (code == H::immediate)
    {
      if (current_handler.is_immediate())
      {
        Dout(dc::notice, "Aborting because calling yield() while the current handler is <immediate> is not allowed (yield_code = " << std::hex << m_yield_code << ").");
        abort();
        return;
      }
      yield();
    }
    else
      yield(get_handler(code));
  }

  void print_call_yield(Handler current_handler)
  {
    Dout(dc::notice, "Current handler: " << current_handler);
    H code = (H)((m_yield_code >> 4) & 0x3);
    if (code == H::immediate)
      yield();
    else
      yield(get_handler(code));
  }

  void timed_out();

 protected:
  typedef AIStatefulTask direct_base_type;              // The base class of this task.

  // The different states of the task.
  enum hello_world_state_type {
    ThreadPoolYieldTest_start = direct_base_type::state_end,     // Mandatory first value.
    ThreadPoolYieldTest_print,
    ThreadPoolYieldTest_done,
  };

 public:
  static state_type constexpr state_end = ThreadPoolYieldTest_done + 1;        // Must be one passed the last state.
  ThreadPoolYieldTest();

 protected: // The destructor must be protected.
  // Override virtual functions of the base class.
  ~ThreadPoolYieldTest() override;
  char const* state_str_impl(state_type run_state) const override;
  void initialize_impl() override;
  void multiplex_impl(state_type run_state) override;
};

ThreadPoolYieldTest::ThreadPoolYieldTest() : CWDEBUG_ONLY(AIStatefulTask(true))
{
  DoutEntering(dc::statefultask, "ThreadPoolYieldTest::ThreadPoolYieldTest()");
}

ThreadPoolYieldTest::~ThreadPoolYieldTest()
{
  DoutEntering(dc::statefultask, "ThreadPoolYieldTest::~ThreadPoolYieldTest()");
}

// Allow human readable states.
char const* ThreadPoolYieldTest::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of hello_world_state_type.
    AI_CASE_RETURN(ThreadPoolYieldTest_start);
    AI_CASE_RETURN(ThreadPoolYieldTest_print);
    AI_CASE_RETURN(ThreadPoolYieldTest_done);
  }

  AI_NEVER_REACHED
};

void ThreadPoolYieldTest::timed_out()
{
  DoutEntering(dc::notice, "ThreadPoolYieldTest::timed_out()");
  sub_state_type::wat(mSubState)->idle = 1;     // HACK - don't use this.
  abort();
}

void ThreadPoolYieldTest::initialize_impl()
{
  m_timer.start(threadpool::Interval<100, std::chrono::milliseconds>(), [this](){ timed_out(); });
  set_state(ThreadPoolYieldTest_start);
}

void ThreadPoolYieldTest::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case ThreadPoolYieldTest_start:              // First state is the entry point.
    {
      ASSERT(!yield_if_not(get_handler(run_handler())));
      Handler current_handler = get_handler(run_handler());
      set_state(ThreadPoolYieldTest_print);      // Continue in state ThreadPoolYieldTest_print.
      start_call_yield(current_handler);
      break;
    }
    case ThreadPoolYieldTest_print:
    {
      ASSERT(!yield_if_not(get_handler(start_handler())));
      Handler current_handler = get_handler(start_handler());
      set_state(ThreadPoolYieldTest_done);
      print_call_yield(current_handler);
      break;
    }
    case ThreadPoolYieldTest_done:
      ASSERT(!yield_if_not(get_handler(print_handler())));
      finish();                         // and finish.
      break;
  }
}

} // namespace task

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()");

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  AIQueueHandle low_priority_queue = thread_pool.new_queue(8);
  AIQueueHandle high_priority_queue = thread_pool.new_queue(8);

  try
  {
    evio::EventLoop event_loop(low_priority_queue);
    AIEngine engine("main engine", 2.0);
    Dout(dc::notice, "engine = " << &engine);

    using task::ThreadPoolYieldTest;
    using H = ThreadPoolYieldTest::H;

    for (int yield_code = 0; yield_code < 64; ++yield_code)
    {
      auto task = statefultask::create<task::ThreadPoolYieldTest>();
      task->initialize_handler_array(&engine, low_priority_queue, high_priority_queue);
      task->set_yield_code(yield_code);
      bool test_finished = false;
      // Let debug output of other threads print their initialization stuff first.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (task->run_handler() == H::immediate)
        task->run(
            [&](bool CWDEBUG_ONLY(success)){
            test_finished = true;
            Dout(dc::notice, "Inside the call-back (" <<
                (success ? "success" : "failure") << ").");
        });
      else
        task->run(
            task->get_handler(task->run_handler()),
            [&](bool CWDEBUG_ONLY(success)){
            test_finished = true;
            Dout(dc::notice, "Inside the call-back (" <<
                (success ? "success" : "failure") << ").");
        });

      // Add an artificial main loop here.
      while (!test_finished)
      {
        // Run tasks from a known point in some libraries main loop.
        // This 'synchronizes' the task with the code of that particular library.
        engine.mainloop();
        // Pretend we're doing something else too.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()");
}
