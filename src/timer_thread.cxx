#include "sys.h"
#include "statefultask/AIThreadPool.h"
#include "statefultask/Timer.h"
#include "debug.h"

class Callback
{
 private:
  int m_nr;
  AIQueueHandle m_queue_handle;

 public:
  Callback(int nr, AIQueueHandle queue_handle) : m_nr(nr), m_queue_handle(queue_handle) { }
  void callback() const;
};

void Callback::callback() const
{
  Dout(dc::notice, "Timer " << m_nr << " expired.");
  int added_tasks = 0;
  auto queues_access = AIThreadPool::instance().queues_read_access();
  auto& queue = AIThreadPool::instance().get_queue(queues_access, m_queue_handle);
  {
    auto queue_access = queue.producer_access();
    int length = queue_access.length();
    if (length < 32)
    {
      added_tasks = std::min(32 - length, m_nr + 1);
      for (int i = 0; i < added_tasks; ++ i)
      {
        queue_access.move_in([this, i](){ Dout(dc::notice, "Pool executed task #" << i << " added by callback of timer " << m_nr); return false; });
        Dout(dc::notice, "Added task #" << i << " to queue " << m_queue_handle);
      }
    }
  }
  for (int i = 0; i < added_tasks; ++ i)
    queue.notify_one();
}

#define SIMPLE 0

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

#if !SIMPLE
  AIThreadPool thread_pool;
#else
  AIThreadPool thread_pool(1, 1);
#endif
  AIQueueHandle high_priority = thread_pool.new_queue(32);
#if !SIMPLE
  AIQueueHandle mid_priority = thread_pool.new_queue(32);
  AIQueueHandle low_priority = thread_pool.new_queue(32);
#else
  AIQueueHandle mid_priority = high_priority;
  AIQueueHandle low_priority = high_priority;
#endif

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::array<Callback, 10> callbacks = {
    Callback(0, high_priority),
    Callback(1, mid_priority),
    Callback(2, low_priority),
    Callback(3, high_priority),
    Callback(4, mid_priority),
    Callback(5, low_priority),
    Callback(6, high_priority),
    Callback(7, mid_priority),
    Callback(8, low_priority),
    Callback(9, high_priority),
  };

  std::array<std::function<void()>, 10> pf = {
    std::bind(&Callback::callback, &callbacks[0]),
    std::bind(&Callback::callback, &callbacks[1]),
    std::bind(&Callback::callback, &callbacks[2]),
    std::bind(&Callback::callback, &callbacks[3]),
    std::bind(&Callback::callback, &callbacks[4]),
    std::bind(&Callback::callback, &callbacks[5]),
    std::bind(&Callback::callback, &callbacks[6]),
    std::bind(&Callback::callback, &callbacks[7]),
    std::bind(&Callback::callback, &callbacks[8]),
    std::bind(&Callback::callback, &callbacks[9]),
  };

  std::array<statefultask::Timer, 10> timers = {
    pf[0], pf[1], pf[2], pf[3], pf[4], pf[5], pf[6], pf[7], pf[8], pf[9]
  };

  timers[0].start(statefultask::Interval<1, std::chrono::seconds>());
#if !SIMPLE
  timers[1].start(statefultask::Interval<2, std::chrono::seconds>());
  timers[2].start(statefultask::Interval<3, std::chrono::seconds>());
  timers[3].start(statefultask::Interval<4, std::chrono::seconds>());
  timers[4].start(statefultask::Interval<5, std::chrono::seconds>());
  timers[5].start(statefultask::Interval<6, std::chrono::seconds>());
  timers[6].start(statefultask::Interval<7, std::chrono::seconds>());
  timers[7].start(statefultask::Interval<8, std::chrono::seconds>());
  timers[8].start(statefultask::Interval<9, std::chrono::seconds>());
  timers[9].start(statefultask::Interval<10, std::chrono::seconds>());
#endif

  std::this_thread::sleep_for(std::chrono::seconds(11));
  std::cout << "Leaving main()\n" << std::endl;
}
