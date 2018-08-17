#include "sys.h"
#include "debug.h"
#include "resolver-task/AIResolver.h"
#include "statefultask/AIThreadPool.h"

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler __attribute__ ((unused)) = thread_pool.new_queue(queue_capacity);

  auto handle = AIResolver::instance().request("www.google.com", "www");
  auto handle2 = AIResolver::instance().request("www.google.com", "www");

  // Wait till the request is handled.
  while (!handle->is_ready())
    ;

  std::cout << "Result: " << handle->get_result() << std::endl;
  handle.reset();

  Dout(dc::notice, "Leaving main()...");
}
