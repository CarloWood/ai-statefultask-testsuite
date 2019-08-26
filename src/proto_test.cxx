#include "sys.h"
#include "resolver-task/Resolver.h"
#include "evio/EventLoop.h"
#include "cwds/benchmark.h"
#include "debug.h"

using resolver::Resolver;

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

  Debug(dc::warning.off());

  std::array<char const*, 256> strings;

  stopwatch.start();
  for (int proto_nr = 0; proto_nr < 256; ++proto_nr)
    strings[proto_nr] = Resolver::instance().protocol_str(proto_nr);
  stopwatch.stop();

  uint64_t diff_cycles = stopwatch.diff_cycles();
  diff_cycles -= stopwatch.s_stopwatch_overhead;
  double milliseconds = diff_cycles / 3612059.05;
  std::cout << "Used time (uncached): " << milliseconds << " ms." << std::endl;

  stopwatch.start();
  for (int proto_nr = 0; proto_nr < 256; ++proto_nr)
  {
    [[maybe_unused]] char const* str = Resolver::instance().protocol_str(proto_nr);
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
    char const* str = Resolver::instance().protocol_str(proto_nr);
    int protocol = Resolver::instance().protocol(str);
    if (protocol != 0)
    {
      std::cout << "Protocol " << protocol << " = " << str << std::endl;
    }
    ASSERT(((str == nullptr || strcmp(str, "unknown") == 0) && protocol == 0) || protocol == proto_nr);
  }

  Debug(dc::warning.on());

  // Terminate application.
  Resolver::instance().close();
  event_loop.join();
}
