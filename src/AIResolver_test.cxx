#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "resolver-task/Resolver.h"
#include "resolver-task/dns/src/dns.h"

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  // Initialize the IO event loop thread.
  EventLoopThread::instance().init(handler);
  // Initialize the async hostname resolver.
  resolver::Resolver& resolver{resolver::Resolver::instance()};
  resolver.init(true);

  auto handle = resolver.getaddrinfo("irc.undernet.org", "ircd", resolver::AddressInfoHints(AI_CANONNAME));
  //auto handle2 = Resolver::instance().request("www.google.com", "www");

  // Wait till the request is handled.
  while (!handle->is_ready())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (handle->success())
    std::cout << "Result: " << handle->get_result() << std::endl;
  else
    std::cerr << "Failure: " << handle->get_error() << '.' << std::endl;
  handle.reset();

  // Terminate application.
  resolver.close();
  EventLoopThread::instance().terminate();
  Dout(dc::notice, "Leaving main()...");
}
