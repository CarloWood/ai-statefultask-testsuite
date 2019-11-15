#include "sys.h"
#include "threadsafe/Gate.h"
#include "threadpool/AIThreadPool.h"
#include "statefultask/AIStatefulTaskMutex.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#include "cwds/benchmark.h"
#include <chrono>

constexpr int queue_capacity = 100032; //32;
constexpr int number_of_tasks = 100000;

class MyTask : public AIStatefulTask
{
 protected:
  //! The base class of this task.
  using direct_base_type = AIStatefulTask;

  //! The different states of the stateful task.
  enum my_task_state_type {
    MyTask_call_lock = direct_base_type::max_state,
    MyTask_locked,
    MyTask_critical_area,
    MyTask_done
  };

 public:
  //! One beyond the largest state of this task.
  static state_type constexpr max_state = MyTask_done + 1;

 public:
  MyTask(CWDEBUG_ONLY(bool debug = false) ) CWDEBUG_ONLY(: AIStatefulTask(debug))
    { DoutEntering(dc::statefultask(mSMDebug), "MyTask() [" << (void*)this << "]"); }

  int get_result() const { return 0; }

 protected:
  //! Call finish() (or abort()), not delete.
  ~MyTask() override { DoutEntering(dc::statefultask(mSMDebug), "~MyTask() [" << (void*)this << "]"); }

  //! Implemenation of state_str for run states.
  char const* state_str_impl(state_type run_state) const override;

  //! Handle mRunState.
  void multiplex_impl(state_type run_state) override;

 private:
  //void done();
};

char const* MyTask::state_str_impl(state_type run_state) const
{
  switch (run_state)
  {
    AI_CASE_RETURN(MyTask_call_lock);
    AI_CASE_RETURN(MyTask_locked);
    AI_CASE_RETURN(MyTask_critical_area);
    AI_CASE_RETURN(MyTask_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
}

AIStatefulTaskMutex mutex;

std::atomic<int> m_inside_critical_area = ATOMIC_VAR_INIT(0);
std::atomic<int> m_locked = ATOMIC_VAR_INIT(0);
std::atomic<int> finished_counter = ATOMIC_VAR_INIT(0);
bool first = true;

void MyTask::multiplex_impl(state_type state)
{
  switch (state)
  {
    case MyTask_call_lock:
      set_state(MyTask_locked);
      m_locked++;
      if (!mutex.lock(this, 1))
      {
        wait(1);
        break;
      }
      [[fallthrough]];
    case MyTask_locked:
      ASSERT(m_inside_critical_area++ == 0);
      Dout(dc::notice, "Locked! [" << this << "]");
      set_state(MyTask_critical_area);
      break;
    case MyTask_critical_area:
      if (first)
      {
        while (m_locked < number_of_tasks)
        {
//          std::cout << m_locked << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::cout << m_locked << std::endl;
        first = false;
      }

      ASSERT(m_inside_critical_area-- == 1);
      mutex.unlock();
      m_locked--;
      Dout(dc::notice, "Unlocked! [" << this << "]");
      set_state(MyTask_done);
      break;
    case MyTask_done:
      finish();
      break;
  }
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()...");

  AIMemoryPagePool mpp;

  benchmark::Stopwatch sw;
  sw.start();

  // Pre-allocate memory in the memory pools.
  std::vector<void*> blocks;
  utils::MemoryPagePool::blocks_t const required_pool_blocks = 21000000 / mpp.instance().block_size();
  while (mpp.instance().pool_blocks() < required_pool_blocks)
    blocks.push_back(AIStatefulTaskMutex::s_node_memory_resource.allocate(0));
  for (auto ptr : blocks)
    AIStatefulTaskMutex::s_node_memory_resource.deallocate(ptr);

  sw.stop();
  std::cout << "Ran for " << (sw.diff_cycles() / 3612059050.0) << " seconds." << std::endl;

  AIThreadPool thread_pool;

  try
  {
//    evio::EventLoop event_loop(handler);

    // Allow the main thread to wait until the test finished.
    aithreadsafe::Gate test_finished;
    AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

    std::vector<boost::intrusive_ptr<MyTask>> tasks;
    for (int n = 0; n < number_of_tasks; ++n)
      tasks.emplace_back(new MyTask(CWDEBUG_ONLY(true)));

    sw.start();

    for (int i = 0; i < number_of_tasks; ++i)
    {
      tasks[i]->run(handler, [&test_finished COMMA_CWDEBUG_ONLY(i)](bool success){
            if (!success)
              Dout(dc::warning, "MyTask was aborted.");
            else
            {
              Dout(dc::notice, "MyTask " << i << " finished.");
              if (finished_counter++ == number_of_tasks - 1)
                test_finished.open();
            }
          });
    }
    Dout(dc::notice, "Done adding " << number_of_tasks << " to the thread pool queue.");

    // Wait until the test is finished.
    test_finished.wait();

    sw.stop();

    ASSERT(m_inside_critical_area == 0);
    ASSERT(m_locked == 0);

    std::cout << "Ran for " << (sw.diff_cycles() / 3612059050.0) << " seconds." << std::endl;

    // Terminate application.
//    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}
