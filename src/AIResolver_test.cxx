#include "sys.h"
#include "debug.h"
#include "resolver-task/AIResolver.h"
#include "statefultask/AIThreadPool.h"
#include "resolver-task/dns/src/dns.h"

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler __attribute__ ((unused)) = thread_pool.new_queue(queue_capacity);

  // Initialize the IO event loop thread.
  EventLoopThread::instance().init(handler);

  auto handle = AIResolver::instance().request("irc.undernet.org", "ircd");
  //auto handle2 = AIResolver::instance().request("www.google.com", "www");

  // Wait till the request is handled.
  int c = 0;
  while (!handle->is_ready() && ++c < 1000)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  std::cout << "*Result: " << handle->get_result() << std::endl;
  handle.reset();

  // Terminate application.
  AIResolver::instance().close();
  EventLoopThread::instance().terminate();
  Dout(dc::notice, "Leaving main()...");
}
