#include "sys.h"
#include "debug.h"
#include "threadpool/AIThreadPool.h"
#include "resolver-task/AddrInfoLookup.h"

int constexpr queue_capacity = 32;
using namespace resolver;

#ifndef CWDEBUG
pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);
  // Initialize the IO event loop thread.
  evio::EventLoop event_loop(handler);
  // Initialize the async hostname resolver.
  Resolver::instance().init(handler, true);

  auto handle2 = Resolver::instance().getaddrinfo("irc.undernet.org", 6668, AddressInfoHints(AI_CANONNAME));
  auto handle3 = Resolver::instance().getaddrinfo("www.google.com", "www", AddressInfoHints(AI_CANONNAME));
  auto handle = Resolver::instance().getaddrinfo("irc.undernet.org", "ircd", AddressInfoHints(AI_CANONNAME, AF_UNSPEC, 0 /*, IPPROTO_TCP*/));

  // Wait till the requests are handled.
  while (!handle3->is_ready())
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT(handle->is_ready() && handle2->is_ready());

  if (handle->success())
  {
    pthread_mutex_lock(&cout_mutex);
    std::cout << "Result: port = " << handle->get_port() << "; IP#'s = " << handle->get_result() << std::endl;
    pthread_mutex_unlock(&cout_mutex);
  }
  else
    std::cerr << "Failure: " << handle->get_error() << '.' << std::endl;
  if (handle2->success())
  {
    pthread_mutex_lock(&cout_mutex);
    std::cout << "Result2: port = " << handle2->get_port() << "; IP#'s = " << handle2->get_result() << std::endl;
    pthread_mutex_unlock(&cout_mutex);
  }
  else
    std::cerr << "Failure2: " << handle2->get_error() << '.' << std::endl;
  if (handle3->success())
  {
    pthread_mutex_lock(&cout_mutex);
    std::cout << "Result3: port = " << handle3->get_port() << "; IP#'s = " << handle3->get_result() << std::endl;
    pthread_mutex_unlock(&cout_mutex);
  }
  else
    std::cerr << "Failure3: " << handle3->get_error() << '.' << std::endl;

  handle.reset();
  handle2.reset();
  handle3.reset();

  // Terminate application.
  Resolver::instance().close();
  event_loop.join();
  Dout(dc::notice, "Leaving main()...");
}
