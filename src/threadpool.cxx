#include "sys.h"
#include "threadpool/AIThreadPool.h"
#include "threadsafe/Condition.h"
#include "debug.h"
#include <chrono>

int constexpr capacity = 1300;
int constexpr loop_size = 1000000;
int constexpr modulo = 5;

static int volatile vv;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  std::atomic_int count{0};
  std::atomic_int empty{0};
  std::atomic_int full{0};
  {
    AIThreadPool thread_pool(6);
    AIQueueHandle queue_handle1 = thread_pool.new_queue(capacity);
    {
      auto queues_access = thread_pool.queues_read_access();

      double delay = 200.0;
      for (int n = 0; n < loop_size; ++n)
      {
        // Note the reference (&) here (it won't compile without it).
        auto& queue = thread_pool.get_queue(queues_access, queue_handle1);
        int length;
        {
          auto access = queue.producer_access();
          length = access.length();
          if (length < capacity) // Buffer not full?
            access.move_in([&count](){ int c = count++; return c % modulo == 0; });
        }
        if (AI_UNLIKELY(length == capacity))
        {
          Dout(dc::notice, "Queue was full (n = " << n << "; delay = " << delay << "; empty = " << empty << ").");
          full++;
        }
        else
          queue.notify_one();
        if (length == 0)
          empty++;

        delay *= 1.0 + (0.002 * length / capacity - 0.001);

        size_t cnt = delay;
        for (size_t i = 0; i < cnt; ++i)
          vv = 1;
      }
      // Wait for the queue to be entirely processed.
      {
        aithreadsafe::Condition finished;
        auto queues_access = thread_pool.queues_read_access();
        auto& queue = thread_pool.get_queue(queues_access, queue_handle1);
        int length;
        do
        {
          auto access = queue.producer_access();
          length = access.length();
          if (length < capacity) // Buffer not full?
            access.move_in([&finished](){ finished.signal(); return false; });
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        while (AI_UNLIKELY(length == capacity));
        queue.notify_one();
        finished.wait();
      }
      Dout(dc::notice, "delay = " << delay << "; empty = " << empty);
    }
  }
  std::cout << "Added " << count << " tasks to the queue." << std::endl;
  std::cout << "Expected: " << ((loop_size - full) * modulo / (modulo - 1)) << std::endl;
}
