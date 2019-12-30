#include "sys.h"
#include "debug.h"
#include "cwds/benchmark.h"
#include "resolver-task/AddressInfo.h"
#include <farmhash.h>
#include <vector>

struct Result
{
  int m_cpu;
  uint64_t m_start_cycles;
  uint64_t m_diff_cycles;

  void print(uint64_t CWDEBUG_ONLY(time_offset)) const
  {
    Dout(dc::notice, "Thread on CPU #" << m_cpu << " started running at t = " << (m_start_cycles - time_offset) << " and ran for " << m_diff_cycles / 3612059050.0 << " seconds.");
  }
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  int const cpu = 0;
  int const loopsize = 5000000;

  {
    benchmark::Stopwatch stopwatch(cpu);
    stopwatch.calibrate_overhead(1000, 3);
  }

  eda::FrequencyCounter<uint64_t, 8> fc;
  bool done = false;

  for (int run = 0; run < 1000; ++run)
  {
    benchmark::Stopwatch stopwatch(cpu);
    stopwatch.start();

    std::string s("texture123.secondlife.com");
    resolver::AddressInfoHints hints(AI_CANONNAME, AF_INET6, SOCK_STREAM);

    [[maybe_unused]] uint64_t hash;
    for (int i = 0; i < loopsize; ++i)
    {
      hash = util::Hash64WithSeeds(s.data(), s.length(), 0x9ae16a3b2f90404fULL, hints.hash_seed());
    }

    stopwatch.stop();
    Dout(dc::notice, "Hash is " << std::hex << hash);

    Result result;
    result.m_cpu = cpu;
    result.m_start_cycles = stopwatch.start_cycles();
    result.m_diff_cycles = stopwatch.diff_cycles();
    result.m_diff_cycles -= stopwatch.s_stopwatch_overhead;

    uint64_t bucket = result.m_diff_cycles / 3612059.05;      // milliseconds.
    Dout(dc::notice, "Measured " << result.m_diff_cycles << " clock cycles (" << bucket << " milliseconds per " << loopsize << " Hash calls).\n");
    if (fc.add(bucket))
    {
      done = true;
      break;
    }
  }

  fc.print_on(std::cout);
  if (done)
    std::cout << "Result: " << (fc.result().m_cycles * 1000000.0 / loopsize) << " ns." << std::endl;
  else
    std::cout << "Result: " << fc.average() << std::endl;
}
