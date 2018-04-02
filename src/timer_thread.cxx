#include "sys.h"
#include "statefultask/AIThreadPool.h"
#include "statefultask/Timer.h"
#include "debug.h"

void callback1()
{
  Dout(dc::notice, "Timer 1 expired.");
}

void callback2()
{
  Dout(dc::notice, "Timer 2 expired.");
}

void callback3()
{
  Dout(dc::notice, "Timer 3 expired.");
}

void callback4()
{
  Dout(dc::notice, "Timer 4 expired.");
}

void callback5()
{
  Dout(dc::notice, "Timer 5 expired.");
}

void callback6()
{
  Dout(dc::notice, "Timer 6 expired.");
}

void callback7()
{
  Dout(dc::notice, "Timer 7 expired.");
}

void callback8()
{
  Dout(dc::notice, "Timer 8 expired.");
}

void callback9()
{
  Dout(dc::notice, "Timer 9 expired.");
}

void callback10()
{
  Dout(dc::notice, "Timer 10 expired.");
}

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  std::function<void()> pf[10] = {
    callback1, callback2, callback3, callback4, callback5,
    callback6, callback7, callback8, callback9, callback10,
  };
  std::array<statefultask::Timer, 10> timers = {
    pf[0], pf[1], pf[2], pf[3], pf[4], pf[5], pf[6], pf[7], pf[8], pf[9]
  };

  timers[0].start(statefultask::Interval<1, std::chrono::seconds>());
  timers[1].start(statefultask::Interval<2, std::chrono::seconds>());
  timers[2].start(statefultask::Interval<3, std::chrono::seconds>());
  timers[3].start(statefultask::Interval<4, std::chrono::seconds>());
  timers[4].start(statefultask::Interval<5, std::chrono::seconds>());
  timers[5].start(statefultask::Interval<6, std::chrono::seconds>());
  timers[6].start(statefultask::Interval<7, std::chrono::seconds>());
  timers[7].start(statefultask::Interval<8, std::chrono::seconds>());
  timers[8].start(statefultask::Interval<9, std::chrono::seconds>());
  timers[9].start(statefultask::Interval<10, std::chrono::seconds>());

  AIThreadPool thread_pool;
  AIQueueHandle high_priority = thread_pool.new_queue(32);
  AIQueueHandle mid_priority = thread_pool.new_queue(32);
  AIQueueHandle low_priority = thread_pool.new_queue(32);

  std::this_thread::sleep_for(std::chrono::seconds(11));
  std::cout << "Leaving main()\n" << std::endl;
}
