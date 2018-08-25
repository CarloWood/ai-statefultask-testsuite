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
  // Initialize the async hostname resolver.
  AIResolver::instance().init(true);

  auto handle = AIResolver::instance().getaddrinfo("irc.undernet.org", "6669", evio::AddressInfoHints(AI_CANONNAME));
  //auto handle2 = AIResolver::instance().request("www.google.com", "www");

  // Wait till the request is handled.
  while (!handle->is_ready())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (handle->success())
    std::cout << "Result: " << handle->get_result() << std::endl;
  else
    std::cerr << "Failure: " << handle->get_error() << '.' << std::endl;
  handle.reset();

  // Terminate application.
  AIResolver::instance().close();
  EventLoopThread::instance().terminate();
  Dout(dc::notice, "Leaving main()...");
}
