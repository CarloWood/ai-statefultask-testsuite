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
    stopwatch.calibrate_overhead(1000, 3);
  }
  benchmark::Stopwatch stopwatch(cpu);

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(32);
  // Initialize the IO event loop thread.
  evio::EventLoop event_loop(handler);
  // Initialize the async hostname resolver.
  Resolver::instance().init(handler, false);

  for (int n = 0; n < 2; ++n)
  {
    stopwatch.start();

    Service echo_udp("echo", IPPROTO_UDP);
    [[maybe_unused]] uint16_t echo_udp_port = Resolver::instance().port(echo_udp);
    ASSERT(echo_udp_port == 7);

    Service echo_ddp("echo", IPPROTO_DDP);
    [[maybe_unused]] uint16_t echo_ddp_port = Resolver::instance().port(echo_ddp);
    ASSERT(echo_ddp_port == 4);

    Service echo_any("echo");
    [[maybe_unused]] uint16_t echo_any_port = Resolver::instance().port(echo_any);
    ASSERT(echo_any_port == 7);

    Service www_any("www");
    [[maybe_unused]] uint16_t www_any_port = Resolver::instance().port(www_any);
    ASSERT(www_any_port == 80);

    Service www_udp("www", IPPROTO_UDP);
    [[maybe_unused]] uint16_t www_udp_port = Resolver::instance().port(www_udp);
    ASSERT(www_udp_port == 0);

    stopwatch.stop();

    uint64_t diff_cycles = stopwatch.diff_cycles();
    diff_cycles -= stopwatch.s_stopwatch_overhead;
    double milliseconds = diff_cycles / 3612059.05;
    std::cout << "Used time (" << (n ? "cached" : "uncached") << "): " << milliseconds << " ms." << std::endl;
  }

  // Terminate application.
  Resolver::instance().close();
  event_loop.join();
}
