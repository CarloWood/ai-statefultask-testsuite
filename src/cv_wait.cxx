#include <chrono>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <iomanip>

std::mutex m;
std::condition_variable cv;
std::atomic_bool w = ATOMIC_VAR_INIT(false);
std::chrono::time_point<std::chrono::high_resolution_clock> wait_start;
std::chrono::time_point<std::chrono::high_resolution_clock> wait_end;

void foo()
{
  {
    // 2. Obtain the lock on m.
    std::unique_lock<std::mutex> lk(m);
    // 3. Notify the main thread that we obtained the lock on m.
    w.store(true);
    // Wait a long time until everything is at rest: this thread having the lock on m, the main thread waiting to get it.
    std::this_thread::sleep_for(std::chrono::microseconds(1000));

    // 5. After a while ENTER WAIT!
    wait_start = std::chrono::high_resolution_clock::now();     // Record the time just before entering wait().
#if 1
    cv.wait(lk);                                                // 6. Go idle and unlock m.
#else
    lk.unlock();
    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    lk.lock();
#endif
    // 11. We returned from wait(). We obtained the lock on m again here.
  } // 12. Release the lock on m.
  wait_end = std::chrono::high_resolution_clock::now();         // Record the time after returning from wait().
  // All times are known now. Terminate the thread.
}

int main()
{
  // 1. Start a another thread.
  std::thread foo_thread(&foo);

  // Wait until that thread obtained the lock on m.
  while (!w.load())
    ;

  {
    // 4. Block on m (because foo_thread has it, which is sleeping now).
    std::lock_guard<std::mutex> lk(m);
    // 7. We obtained the lock on m: foo_thread is now in wait() (point 6.)
  } // 8. Immediately release the lock again.
  // 9. CALL notify_one()!
  auto notify_start = std::chrono::high_resolution_clock::now();        // Record the time just before entering notify_one().
  cv.notify_one();
  auto notify_end = std::chrono::high_resolution_clock::now();          // Record the time again just after returning from notify_one().

  // 10. All times have been recorded now, except perhaps wait_end. So, wait until foo_thread completely finished.
  foo_thread.join();

  // 13. Just print out the measurements.
  auto notify_diff = 0.001 * std::chrono::duration_cast<std::chrono::nanoseconds>(notify_end - notify_start).count();
  auto wait_diff = 0.001 * std::chrono::duration_cast<std::chrono::nanoseconds>(wait_end - wait_start).count();
  auto wakeup_diff = 0.001 * std::chrono::duration_cast<std::chrono::nanoseconds>(wait_end - notify_end).count();
  auto delay_diff = 0.001 * std::chrono::duration_cast<std::chrono::nanoseconds>(notify_start - wait_start).count();
  std::cout.precision(1);
  std::cout << "From entering wait() until entering notify_one(): " << std::fixed << delay_diff << " μs." << std::endl;
  std::cout << "From entering notify_one() until returning: " << notify_diff << " μs." << std::endl;
  std::cout << "From leaving notify_one() until leaving wait(): " << wakeup_diff << " μs." << std::endl;
  std::cout << "From entering wait() until returning: " << std::fixed << wait_diff << " μs." << std::endl;
}
