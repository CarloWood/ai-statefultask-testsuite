#include "sys.h"
#include "statefultask/AIThreadPool.h"
#include "debug.h"
#include <chrono>

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  std::atomic_int count{0};
  {
    AIThreadPool thread_pool;
    AIThreadPool::QueueHandle queue_handle = thread_pool.new_queue(8);
    {
      auto queues_access = thread_pool.queues_read_access();

      for (int n = 0; n < 1000000; ++n)
      {
        AIObjectQueue<std::function<void()>> const& queue = thread_pool.get_queue(queues_access, queue_handle);
        auto access = queue.producer_access();
        int length = access.length();
        if (length < 8) // Buffer not full?
          access.move_in([&count](){ Dout(dc::notice, count << ": hello pool!"); count++; });
        else
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    // Give a thread the time to read the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::cout << "Wrote " << count << " times 'hello pool!'" << std::endl;
}
