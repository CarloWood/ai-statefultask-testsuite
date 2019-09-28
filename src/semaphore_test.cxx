#include "sys.h"
#include "threadsafe/Semaphore.h"
#include "debug.h"
#include <thread>
#include <chrono>

void waiter(aithreadsafe::Semaphore& semaphore)
{
  Dout(dc::notice, "Entering waiter()...");
  semaphore.wait();
  Dout(dc::notice, "Leaving waiter()...");
}

void waker(aithreadsafe::Semaphore& semaphore)
{
  Dout(dc::notice, "Entering waker()...");
  semaphore.post(3);
  Dout(dc::notice, "Leaving waker()...");
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()...");

  aithreadsafe::Semaphore semaphore(0);

  std::thread thr1([&](){ Debug(NAMESPACE_DEBUG::init_thread("THREAD1")); waiter(semaphore); });
  std::thread thr2([&](){ Debug(NAMESPACE_DEBUG::init_thread("THREAD2")); waiter(semaphore); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::thread thr3([&](){ Debug(NAMESPACE_DEBUG::init_thread("THREAD3")); waker(semaphore); });
  std::thread thr4([&](){ Debug(NAMESPACE_DEBUG::init_thread("THREAD4")); waker(semaphore); });

  thr1.join();
  thr2.join();
  thr3.join();
  thr4.join();

  Dout(dc::notice, "Leaving main()...");
}
