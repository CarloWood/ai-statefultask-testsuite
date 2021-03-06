#include "sys.h"
#include "threadpool/AIThreadPool.h"
#include "threadpool/Timer.h"
#include "debug.h"

int const queue_size = 32;

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
  Dout(dc::notice|flush_cf, "Timer " << m_nr << " expired.");
  int added_tasks = 0;
  auto queues_access = AIThreadPool::instance().queues_read_access();
  auto& queue = AIThreadPool::instance().get_queue(queues_access, m_queue_handle);
  {
    auto queue_access = queue.producer_access();
    int length = queue_access.length();
    if (length < queue_size)
    {
      added_tasks = std::min(queue_size - length, 10 * (m_nr + 1));
      for (int i = 0; i < added_tasks; ++ i)
      {
        queue_access.move_in([CWDEBUG_ONLY(this, i)](){ Dout(dc::notice, "Pool executed task #" << i << " added by callback of timer " << m_nr); return false; });
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
  Debug(NAMESPACE_DEBUG::init());

#if !SIMPLE
  AIThreadPool thread_pool;
#else
  AIThreadPool thread_pool(1, 1);
#endif
  AIQueueHandle high_priority = thread_pool.new_queue(queue_size);
#if !SIMPLE
  AIQueueHandle mid_priority = thread_pool.new_queue(queue_size);
  AIQueueHandle low_priority = thread_pool.new_queue(queue_size);
#else
  AIQueueHandle mid_priority = high_priority;
  AIQueueHandle low_priority = high_priority;
#endif

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

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

  std::array<threadpool::Timer, 10> timers = {
    pf[0], pf[1], pf[2], pf[3], pf[4], pf[5], pf[6], pf[7], pf[8], pf[9]
  };

  timers[0].start(threadpool::Interval<1, std::chrono::microseconds>());
#if !SIMPLE
  timers[1].start(threadpool::Interval<2, std::chrono::microseconds>());
  timers[2].start(threadpool::Interval<3, std::chrono::microseconds>());
  timers[3].start(threadpool::Interval<4, std::chrono::microseconds>());
  timers[4].start(threadpool::Interval<5, std::chrono::microseconds>());
  timers[5].start(threadpool::Interval<6, std::chrono::microseconds>());
  timers[6].start(threadpool::Interval<7, std::chrono::microseconds>());
  timers[7].start(threadpool::Interval<8, std::chrono::microseconds>());
  timers[8].start(threadpool::Interval<9, std::chrono::microseconds>());
  timers[9].start(threadpool::Interval<10, std::chrono::microseconds>());
#endif

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << "Leaving main()\n" << std::endl;
}
