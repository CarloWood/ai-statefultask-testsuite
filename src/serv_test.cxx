#include "sys.h"
#include "debug.h"
#include "cwds/benchmark.h"
#include "resolver-task/Resolver.h"

using namespace resolver;

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  int const cpu = 0;

  {
    benchmark::Stopwatch stopwatch(cpu);
    stopwatch.calibrate_overhead();
  }
  benchmark::Stopwatch stopwatch(cpu);

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(32);
  // Initialize the IO event loop thread.
  EventLoopThread::instance().init(handler);
  // Initialize the async hostname resolver.
  Resolver::instance().init(false);

  for (int n = 0; n < 2; ++n)
  {
    stopwatch.start();

    Service echo_udp("echo", IPPROTO_UDP);
    in_port_t echo_udp_port = Resolver::instance().port(echo_udp);
    ASSERT(echo_udp_port == 7);

    Service echo_ddp("echo", IPPROTO_DDP);
    in_port_t echo_ddp_port = Resolver::instance().port(echo_ddp);
    ASSERT(echo_ddp_port == 4);

    Service echo_any("echo");
    in_port_t echo_any_port = Resolver::instance().port(echo_any);
    ASSERT(echo_any_port == 7);

    Service www_any("www");
    in_port_t www_any_port = Resolver::instance().port(www_any);
    ASSERT(www_any_port == 80);

    Service www_udp("www", IPPROTO_UDP);
    in_port_t www_udp_port = Resolver::instance().port(www_udp);
    ASSERT(www_udp_port == 0);

    stopwatch.stop();

    uint64_t diff_cycles = stopwatch.diff_cycles();
    diff_cycles -= stopwatch.s_stopwatch_overhead;
    double milliseconds = diff_cycles / 3612059.05;
    std::cout << "Used time (" << (n ? "cached" : "uncached") << "): " << milliseconds << " ms." << std::endl;
  }

  // Terminate application.
  Resolver::instance().close();
  EventLoopThread::instance().terminate();
}
