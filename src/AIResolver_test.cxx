#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "resolver-task/Lookup.h"

int constexpr queue_capacity = 32;
using namespace resolver;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);
  // Initialize the IO event loop thread.
  EventLoopThread::instance().init(handler);
  // Initialize the async hostname resolver.
  Resolver::instance().init(false);

  {
  auto handle2 = Resolver::instance().getaddrinfo("irc.undernet.org", 6668, AddressInfoHints(AI_CANONNAME));
  }
  auto handle = Resolver::instance().getaddrinfo("irc.undernet.org", "ircd", AddressInfoHints(AI_CANONNAME, AF_UNSPEC, 0 /*, IPPROTO_TCP*/));

  // Wait till the request is handled.
  while (!handle->is_ready())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  if (handle->success())
  {
    pthread_mutex_lock(&cout_mutex);
    std::cout << "Result: port = " << handle->get_port() << "; IP#'s = " << handle->get_result() << std::endl;
    //std::cout << "Result2: port = " << handle2->get_port() << "; IP#'s = " << handle2->get_result() << std::endl;
    pthread_mutex_unlock(&cout_mutex);
  }
  else
    std::cerr << "Failure: " << handle->get_error() << '.' << std::endl;
  handle.reset();

  // Terminate application.
  Resolver::instance().close();
  EventLoopThread::instance().terminate();
  Dout(dc::notice, "Leaving main()...");
}
