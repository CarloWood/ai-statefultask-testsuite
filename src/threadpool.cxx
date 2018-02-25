#include "sys.h"
#include "statefultask/AIThreadPool.h"
#include "debug.h"
#include <chrono>

int constexpr capacity = 1300;

static int volatile vv;

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  std::atomic_int count{0};
  std::atomic_int empty{0};
  {
    AIThreadPool thread_pool(3);
    AIThreadPool::QueueHandle queue_handle1 = thread_pool.new_queue(capacity);
    AIThreadPool::QueueHandle queue_handle2 = thread_pool.new_queue(capacity);
    AIThreadPool::QueueHandle queue_handle3 = thread_pool.new_queue(capacity);
    {
      auto queues_access = thread_pool.queues_read_access();

      double delay = 200.0;
      for (int n = 0; n < 1000000; ++n)
      {
        // Note the reference (&) here (it won't compile without it).
        auto& queue = thread_pool.get_queue(queues_access, queue_handle3);
        int length;
        {
          auto access = queue.producer_access();
          length = access.length();
          if (length < capacity) // Buffer not full?
            access.move_in([&count](){ count++; });
        }
        if (AI_UNLIKELY(length == capacity))
        {
          Dout(dc::notice, "Queue was full (n = " << n << "; delay = " << delay << "; empty = " << empty << ").");
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
      Dout(dc::notice, "delay = " << delay << "; empty = " << empty);
    }

    // Give a thread the time to read the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::cout << "Wrote " << count << " times 'hello pool!'" << std::endl;
}
