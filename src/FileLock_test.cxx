#include "sys.h"
#include "filelock-task/FileLock.h"
#include "statefultask/AIEngine.h"
#include "evio/EventLoop.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"

struct TaskMutex
{
 private:
  std::mutex m_mutex;
  AIStatefulTask* m_write_owner;
  int m_readers;

 public:
  TaskMutex() : m_write_owner(nullptr), m_readers(0) { }

  void wrlock(AIStatefulTask* owner, bool& success)
  {
    DoutEntering(dc::notice, "TaskMutex::wrlock(" << owner << ")");
    m_mutex.lock();
    success = !m_write_owner && m_readers == 0;
    if (success)
      m_write_owner = owner;
    m_mutex.unlock();
    if (success)
      Dout(dc::notice, "Write lock grabbed successfully!");
    else
      Dout(dc::notice, "Write lock not grabbed successfully!");
  }

  void rdlock(AIStatefulTask* CWDEBUG_ONLY(owner), bool& success)
  {
    DoutEntering(dc::notice, "TaskMutex::rdlock(" << owner << ")");
    m_mutex.lock();
    success = !m_write_owner;
    if (success)
      ++m_readers;
    m_mutex.unlock();
    if (success)
      Dout(dc::notice, "Read lock grabbed successfully!");
    else
      Dout(dc::notice, "Read lock not grabbed successfully!");
  }

  void wr2rdlock()
  {
    // Only call wr2rdlock() after wrlock() returned success.
    ASSERT(m_write_owner != nullptr);
    DoutEntering(dc::notice, "TaskMutex::wr2rdlock() - removing owner " << m_write_owner);
    m_mutex.lock();
    ASSERT(m_readers == 0);
    m_readers = 1;
    m_write_owner = nullptr;
    m_mutex.unlock();
  }

  void wrunlock()
  {
  }

  void rdunlock()
  {
  }

  void reset_owner()
  {
    DoutEntering(dc::notice, "TaskMutex::reset_owner() - removing owner " << m_write_owner);
    // Only call reset_owner() after wrlock() returned success.
    m_mutex.lock();
    ASSERT(m_write_owner != nullptr);
    m_write_owner = nullptr;
    m_mutex.unlock();
  }

  void release_rdlock()
  {
    DoutEntering(dc::notice, "TaskMutex::release_rdlock()");
    m_mutex.lock();
    // Only call release_rdlock() when rdlock() returned success (or wrlock() returned success followed by a call to wr2rdlock()).
    ASSERT(m_readers > 0);
    --m_readers;
    m_mutex.unlock();
  }
};

namespace policy {

struct ReadWriteTask : public aithreadsafe::policy::ReadWrite<TaskMutex>
{
  void wrunlock()
  {
    m_read_write_mutex.reset_owner();
  }

  void rdunlock()
  {
    m_read_write_mutex.release_rdlock();
  }
};

} // namespace policy

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(32);

  using Data_ts = aithreadsafe::Wrapper<int, policy::ReadWriteTask>;
  Data_ts data;

  try
  {
    evio::EventLoop event_loop(handler);
    AIEngine engine("main engine", 2.0);

    FileLock* flock1 = new FileLock("flock1");
    FileLock flock2;
    flock2.set_filename("flock2");

    Dout(dc::notice, "flock2 = " << flock2);

    FileLockAccess file_lock_access1(*flock1);
    delete flock1;
    FileLockAccess file_lock_access2(file_lock_access1);
    AIStatefulTaskNamedMutex task_lock_access(file_lock_access1);

    AIStatefulTaskLockTask* my_task = new AIStatefulTaskLockTask(file_lock_access1);
    AIStatefulTask* ptr = my_task;

    task_lock_access.try_lock(my_task);
    Dout(dc::notice, "task_lock_access = " << task_lock_access);

#if 0
    bool have_lock1;
    {
      Data_ts::rat data_r(data, ptr, have_lock1);
    }

    bool have_lock2;
    {
      Data_ts::rat data_r(data, ptr + 1, have_lock2);
    }

    if (have_lock1)
      data.rdunlock();

    bool have_lock3;
    {
      Data_ts::wat data_w(data, ptr + 1, have_lock3);
    }

    if (have_lock3)
      data.wrunlock();

    {
      Data_ts::wat data_w(data, ptr + 1, have_lock1);
    }

    std::atomic<bool> test_finished = false;
    my_task->run(&engine, [&](bool CWDEBUG_ONLY(success)){ test_finished = true; Dout(dc::notice, "Task finished (" << (success ? "success" : "failure") << ")!"); });

    // Mainloop.
    Dout(dc::notice, "Starting main loop...");
    while (!test_finished)
    {
      engine.mainloop();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif

    // Terminate application.
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}