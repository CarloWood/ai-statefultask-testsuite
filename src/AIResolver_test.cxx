#include "sys.h"
#include "resolver-task/AddrInfoLookup.h"
#include "evio/EventLoop.h"
#include "debug.h"

int constexpr queue_capacity = 32;
using namespace resolver;

#ifdef CWDEBUG
namespace {
auto& cout_mutex = libcwd::cout_mutex;
} // namespace
#else
pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  // Initialize the IO event loop thread and the async hostname resolver.
  evio::EventLoop event_loop(handler);
  resolver::Scope resolver_scope(handler, true);

  auto handle2 = DnsResolver::instance().getaddrinfo("irc.undernet.org", 6668, AddressInfoHints(AI_CANONNAME));
  auto handle3 = DnsResolver::instance().getaddrinfo("www.google.com", "www", AddressInfoHints(AI_CANONNAME));
  auto handle = DnsResolver::instance().getaddrinfo("irc.undernet.org", "ircd", AddressInfoHints(AI_CANONNAME, AF_UNSPEC, 0 /*, IPPROTO_TCP*/));

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
  event_loop.join();
  Dout(dc::notice, "Leaving main()...");
}
