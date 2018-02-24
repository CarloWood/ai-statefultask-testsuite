#include "sys.h"
#include "statefultask/AIThreadPool.h"
#include "debug.h"
#include <chrono>

int constexpr capacity = 128;

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  std::atomic_int count{0};
  {
    AIThreadPool thread_pool;
    AIThreadPool::QueueHandle queue_handle = thread_pool.new_queue(capacity);
    {
      auto queues_access = thread_pool.queues_read_access();

      size_t delay = 1000000;
      for (int n = 0; n < 1000000; ++n)
      {
        // Note the reference (&) here (it won't compile without it).
        auto& queue = thread_pool.get_queue(queues_access, queue_handle);
        int length;
        {
          auto access = queue.producer_access();
          length = access.length();
          if (length < capacity) // Buffer not full?
            access.move_in([&count](){ Dout(dc::notice, count << ": hello pool!"); count++; });
        }
        if (AI_UNLIKELY(length == capacity))
        {
          Dout(dc::notice, "Queue was full (n = " << n << "; delay = " << delay << ").");
          delay *= 1.01;
        }
        else
          queue.notify_one();
        if (length <= 1)
        {
          delay *= 0.99;
        }
        else if (length >= capacity / 2)
        {
          size_t count = delay;
          for (size_t i = 0; i < count; ++i)
            ;
        }
      }
    }

    // Give a thread the time to read the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::cout << "Wrote " << count << " times 'hello pool!'" << std::endl;
}
