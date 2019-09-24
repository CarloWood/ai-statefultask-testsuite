#include "sys.h"
#include "utils/macros.h"
#include "debug.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

class Futex
{
 private:
  int futex(int futex_op, int val, nullptr_t UNUSED_ARG(timeout), int volatile* uaddr2, int val3)
  {
    return syscall(SYS_futex, reinterpret_cast<int volatile*>(&m_word), futex_op, val, nullptr, uaddr2, val3);
  }

  int futex(int futex_op, int val, uint32_t val2, Futex& futex2, int val3)
  {
    return syscall(SYS_futex, reinterpret_cast<int volatile*>(&m_word), futex_op, val,
        reinterpret_cast<struct timespec const*>(static_cast<unsigned long>(val2)), reinterpret_cast<int volatile*>(&futex2.m_word), val3);
  }

 protected:
  std::atomic<int32_t> m_word;

  Futex(int32_t word) : m_word(word) { }

  int wait(int32_t expected) noexcept
  {
    // This operation tests that the value at m_word still contains
    // the expected value, and if so, then sleeps waiting for a FUTEX_WAKE
    // operation on the futex word.
    //
    // The load of the value of the futex word is an atomic memory access
    // (i.e., using atomic machine instructions of the respective
    // architecture). This load, the comparison with the expected value,
    // and starting to sleep are performed atomically and totally ordered
    // with respect to other futex operations on the same futex word.
    //
    // If the thread starts to sleep, it is considered a waiter on this
    // futex word. If the futex value does not match expected, then the call
    // fails immediately with the value -1 and errno set to EAGAIN.
    //
    // Returns 0 if the caller was woken up.
    //
    // Note that a wake-up can also be caused by common futex usage patterns
    // in unrelated code that happened to have previously used the futex word's
    // memory location (e.g., typical futex-based implementations of Pthreads
    // mutexes can cause this under some conditions). Therefore, callers should
    // always conservatively assume that a return value of 0 can mean a
    // spurious wake-up, and use the futex word's value (i.e., the user-space
    // synchronization scheme) to decide whether to continue to block or not.
    return futex(FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
  }

  int wake(int32_t n_threads) noexcept
  {
    // This operation wakes at most n_threads of the waiters that are
    // waiting (e.g., inside FUTEX_WAIT) on m_word.
    //
    // No guarantee is provided about which waiters are awoken (e.g.,
    // a waiter with a higher scheduling priority is not guaranteed
    // to be awoken in preference to a waiter with a lower priority).
    //
    // Returns the number of waiters that were woken up.
    return futex(FUTEX_WAKE_PRIVATE, n_threads, nullptr, nullptr, 0);
  }

  int cmp_requeue(int32_t expected, int32_t n_threads, Futex& target, int32_t target_limit) noexcept
  {
    // This operation first checks whether the location m_word still
    // contains the value expected. If not, the operation fails immediately
    // with the value -1 and errno set to error EAGAIN. Otherwise, the
    // operation wakes up a maximum of n_threads waiters that are waiting
    // on m_word. If there are more than n_threads waiters, then the
    // remaining waiters are removed from the wait queue of this futex and
    // added to the wait queue of the target futex. The val2 argument
    // specifies an upper limit on the number of waiters that are requeued
    // to the futex target.
    //
    // Typical values to specify for n_threads are 0 or 1 (Specifying INT_MAX
    // is not useful, because it would make the FUTEX_CMP_REQUEUE operation
    // equivalent to FUTEX_WAKE). The target_limit is typically either 1 or
    // INT_MAX (Specifying the argument as 0 is not useful, because it would
    // make the FUTEX_CMP_REQUEUE operation equivalent to FUTEX_WAIT).
    //
    // Returns the total number of waiters that were woken up or requeued to target.
    // If this value is greater than n_threads, then the difference is the number
    // of waiters requeued to target.
    return futex(FUTEX_CMP_REQUEUE_PRIVATE, n_threads, target_limit, target, expected);
  }

  int requeue(int32_t n_threads, Futex& target, int32_t target_limit) noexcept
  {
    // This operation performs the same task as FUTEX_CMP_REQUEUE except that no expected check is made.
    return futex(FUTEX_REQUEUE_PRIVATE, n_threads, target_limit, target, 0);
  }

  int wake_op() noexcept
  {
    // FIXME: not implemented yet.
    return -1;
  }

  int wait_bitset(int32_t expected, int32_t bit_mask) noexcept
  {
    // This operation is like FUTEX_WAIT except that bit_mask is used to
    // provide a 32-bit bit mask to the kernel. This bit mask, in which at
    // least one bit must be set, is stored in the kernel-internal state of
    // the waiter. See the description of FUTEX_WAKE_BITSET for further details.
    //
    // Returns 0 if the caller was woken up. See FUTEX_WAIT for how to interpret
    // this correctly in practice.
    return futex(FUTEX_WAIT_BITSET_PRIVATE, expected, nullptr, nullptr, bit_mask);
  }

  int wake_bitset(int32_t n_threads, int32_t bit_mask) noexcept
  {
    // This operation is the same as FUTEX_WAKE except that bit_mask
    // is used to provide a 32-bit bit mask to the kernel.
    // This bit mask, in which at least one bit must be set, is used
    // to select which waiters should be woken up.  The selection is
    // done by a bit-wise AND of bit_mask and the bit mask which is
    // stored in the kernel-internal state of the waiter (the "wait"
    // bit_mask that is set using FUTEX_WAIT_BITSET). All of the waiters
    // for which the result of the AND is nonzero are woken up; the
    // remaining waiters are left sleeping.
    //
    // Returns the number of waiters that were woken up.
    return futex(FUTEX_WAKE_BITSET, n_threads, nullptr, nullptr, bit_mask);
  }
};

class Semaphore : public Futex
{
 private:
  std::atomic_int m_waiters;

 public:
  Semaphore(int32_t count) : Futex(count), m_waiters(0) { }

  bool try_wait() noexcept
  {
    DoutEntering(dc::notice, "Semaphore::try_wait()");
    int32_t count = atomic_load_explicit(&m_word, std::memory_order_relaxed);
    do
    {
      Dout(dc::notice, "count = " << count);
      if (count <= 0)
      {
        Dout(dc::notice, "Returning false because count <= 0");
        return false;
      }
    }
    while (!atomic_compare_exchange_weak_explicit(&m_word, &count, count - 1, std::memory_order_seq_cst, std::memory_order_seq_cst)); // FIXME: memory orders
    Dout(dc::notice, "count " << count << " --> " << (count - 1) << "; returning true.");

    return true;
  }

  void wait() noexcept
  {
    DoutEntering(dc::notice, "Semaphore::wait()");
    int32_t count = atomic_fetch_sub_explicit(&m_word, 1, std::memory_order_relaxed) - 1;
    Dout(dc::notice, "count " << (count + 1) << " --> " << count);
    while (count < 0)
    {
      Dout(dc::notice, "Calling Futex::wait() to try and go to sleep.");
      if (AI_LIKELY(Futex::wait(count) == 0))  // Slept and woke up? FIXME: spurious wake-up?
      {
        Dout(dc::notice, "Leaving Semaphore::wait(): we were woken up!");
        break;                          // We're done.
      }
      ASSERT(errno == EAGAIN);
      // Someone changed the count before we were able to call futex(), try again.
      count = atomic_load_explicit(&m_word, std::memory_order_relaxed);
      Dout(dc::notice, "Futex::wait failed because count was changed to " << count);
    }
  }

  void post() noexcept
  {
    DoutEntering(dc::notice, "Semaphore::post()");
    int32_t prev_count = atomic_fetch_add_explicit(&m_word, 1, std::memory_order_relaxed);
    Dout(dc::notice, "count " << prev_count << " --> " << (prev_count + 1));

    if (prev_count < 0)
    {
      Dout(dc::notice, "Calling Futex::wake(1) because count was less than zero.");
      DEBUG_ONLY(int s =) Futex::wake(1);
      ASSERT(s == 1);
    }
  }
};

void thread1(Semaphore& semaphore)
{
  Debug(NAMESPACE_DEBUG::init_thread("THREAD1"));
  Dout(dc::notice, "Entering thread1()...");
  semaphore.wait();
  Dout(dc::notice, "Leaving thread1()...");
}

void thread2(Semaphore& semaphore)
{
  Debug(NAMESPACE_DEBUG::init_thread("THREAD2"));
  Dout(dc::notice, "Entering thread2()...");
  semaphore.post();
  Dout(dc::notice, "Leaving thread2()...");
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()...");

  Semaphore semaphore(0);

  std::thread thr1([&](){ thread1(semaphore); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::thread thr2([&](){ thread2(semaphore); });

  thr1.join();
  thr2.join();

  Dout(dc::notice, "Leaving main()...");
}
