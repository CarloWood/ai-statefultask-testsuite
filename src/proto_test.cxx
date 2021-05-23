#include "sys.h"
#include "resolver-task/DnsResolver.h"
#include "evio/EventLoop.h"
#include "cwds/benchmark.h"
#include "debug.h"

using resolver::DnsResolver;

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
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  AIQueueHandle handler = thread_pool.new_queue(32);

  // Initialize the IO event loop thread and the async hostname resolver.
  evio::EventLoop event_loop(handler);
  resolver::Scope resolver_scope(handler, false);

  Debug(dc::warning.off());

  std::array<char const*, 256> strings;

  stopwatch.start();
  for (int proto_nr = 0; proto_nr < 256; ++proto_nr)
    strings[proto_nr] = DnsResolver::instance().protocol_str(proto_nr);
  stopwatch.stop();

  uint64_t diff_cycles = stopwatch.diff_cycles();
  diff_cycles -= stopwatch.s_stopwatch_overhead;
  double milliseconds = diff_cycles / 3612059.05;
  std::cout << "Used time (uncached): " << milliseconds << " ms." << std::endl;

  stopwatch.start();
  for (int proto_nr = 0; proto_nr < 256; ++proto_nr)
  {
    [[maybe_unused]] char const* str = DnsResolver::instance().protocol_str(proto_nr);
    // Everything is cached.
    ASSERT(str == strings[proto_nr]);
  }
  stopwatch.stop();

  diff_cycles = stopwatch.diff_cycles();
  diff_cycles -= stopwatch.s_stopwatch_overhead;
  milliseconds = diff_cycles / 3612059.05;
  std::cout << "Used time (cached): " << milliseconds << " ms." << std::endl;

  for (int proto_nr = 0; proto_nr < 256; ++proto_nr)
  {
    char const* str = DnsResolver::instance().protocol_str(proto_nr);
    int protocol = DnsResolver::instance().protocol(str);
    if (protocol != 0)
    {
      std::cout << "Protocol " << protocol << " = " << str << std::endl;
    }
    ASSERT(((str == nullptr || strcmp(str, "unknown") == 0) && protocol == 0) || protocol == proto_nr);
  }

  Debug(dc::warning.on());

  // Terminate application.
  event_loop.join();
}
