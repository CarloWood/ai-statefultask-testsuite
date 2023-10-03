#include "sys.h"
#include "filelock-task/TaskLock.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "evio/EventLoop.h"
#include "utils/debug_ostream_operators.h"

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

struct ReadWriteTask : public threadsafe::policy::ReadWrite<TaskMutex>
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

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  AIQueueHandle handler = thread_pool.new_queue(32);

  struct Integer {
    int value_;
  };
  using Data_ts = threadsafe::Unlocked<Integer, policy::ReadWriteTask>;
  Data_ts data;

  try
  {
    evio::EventLoop event_loop(handler);
    AIEngine engine("main engine", 2.0);

    FileLock flock;
    flock.set_filename("flock1");

    std::filesystem::path file_lock_name = "flock1";
    FileLockAccess file_lock_access(file_lock_name);

    boost::intrusive_ptr<task::TaskLock> my_task1 = new task::TaskLock(file_lock_access);
    boost::intrusive_ptr<task::TaskLock> my_task2 = new task::TaskLock(file_lock_access);

#if 1
    bool have_read_lock1;
    {
      Data_ts::rat data_r(data, my_task1.get(), have_read_lock1);
    }
    ASSERT(have_read_lock1);

    bool have_write_lock2;
    bool have_read_lock2;
    {
      Data_ts::rat data_r(data, my_task2.get(), have_read_lock2);
      ASSERT(have_read_lock2);

      data.rdunlock();

      {
        Data_ts::wat data_w(data, my_task2.get(), have_write_lock2);
      }
      ASSERT(!have_write_lock2);
    }

    data.rdunlock();

    {
      Data_ts::wat data_w(data, my_task2.get(), have_write_lock2);
    }
    ASSERT(have_write_lock2);

    data.wrunlock();
#endif

    std::atomic_int test_finished = 0;
    std::atomic_bool locked_task1 = false;

    my_task1->run(&engine, [&](bool CWDEBUG_ONLY(success)){
        test_finished++;
        locked_task1 = true;
        Dout(dc::notice, "Task1 finished (" << (success ? "success" : "failure") << ")!");
    });

    my_task2->run(&engine, [&](bool CWDEBUG_ONLY(success)){
        test_finished++;
        Dout(dc::notice, "Task2 finished (" << (success ? "success" : "failure") << ")!");
    });

    // Mainloop.
    Dout(dc::notice, "Starting main loop...");
    bool unlocked_task1 = false;
    while (test_finished < 2)
    {
      engine.mainloop();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (locked_task1 && !unlocked_task1)
      {
        my_task1->unlock();
        unlocked_task1 = true;
      }
    }
    my_task2->unlock();

    // Terminate application.
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}
