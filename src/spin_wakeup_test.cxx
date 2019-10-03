#include "sys.h"
#include "debug.h"
#include "threadsafe/Semaphore.h"
#include "utils/cpu_relax.h"
#include "utils/macros.h"
#include "utils/log2.h"
//#include "cwds/benchmark.h"
#include "cwds/gnuplot_tools.h"
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>

constexpr int post_amount = 2;
constexpr int number_of_sleeper_threads = 4;
constexpr int number_of_trigger_threads = 4;
constexpr int number_of_times_to_wait = 3000000;
constexpr int number_of_times_to_wait_per_sleeper = number_of_times_to_wait / number_of_sleeper_threads;
constexpr int number_of_times_to_post_per_trigger = number_of_times_to_wait / number_of_trigger_threads / post_amount;
std::atomic<unsigned long> delay_loop{0};

// post_amount = 1
// fast / slow = 180587 / 2819413 = 0.064 ~ 0
//
// post_amount = 2

namespace threadpool {

// class SpinSemaphore
//
// This semaphore has an internal state (existing of a single 64bit atomic)
// that keeps (atomically) track of the usual 'number of tokens' (ntokens >= 0),
// the 'number of potentially blocked threads' (nwaiters >= 0), and in addition
// to that one bit to keep track of whether or not it has a spinning thread.
//
// ntokens; the number of available tokens: this many threads can return from
//     wait() with adding more tokens, either because they already entered wait()
//     or because they enter wait later.
//
//     Obtaining a token (decrementing ntokens while ntokens > 0) is the last
//     thing a thread does before leaving wait() (expect for the spinner, which
//     might call futex.wake(ntokens - 1) afterwards, see below.
//     While the first thing that post(n) does is incrementing ntokens with n.
//
// nwaiters; an upper limit of the number of threads that are or will end up being
//     blocked by a call to futex.wait() under the assumption that t does not change.
//     Basically this number equals to the number of threads that entered wait()
//     and did not obtain a token yet. It is incremented by one immediately upon
//     entering slow_wait() (unless there are tokens, then ntokens is decremented
//     instead and the thread leaves the function), and at exit of wait() nwaiters
//     and ntokens are atomically both decremented (unless there are no tokens
//     anymore, then neither is decremented and the thread stays inside wait()).
//
// spinner; the thread that "owns" the spinner_mask bit. If that bit is not set
//     there is no spinner and when it is set there is a spinner. The thread that
//     owns the bit is the thread that set it. Hence there can be at most one thread
//     the spinner thread.
//
//     This thread must do the following: while ntokens == 0 it must keep spinning.
//     If ntokens > 0 it must attempt to, atomically, decrease the number of tokens
//     by one and reset the spinner bit, this can only fail when ntokens was changed
//     to zero again, in which case the spinner must return to spinning.
//     If it succeeds it must call futex.wake(ntokens - 1) (iff ntokens > 1),
//     where ntokens is the number of tokens just prior to the successful decrement.
//
//     In other words, tokens added to the atomic while the spinner bit is set causes
//     the spinner to take the responsiblity to wake up up till that many additional
//     threads, if any.

class SpinSemaphore : public aithreadsafe::Futex<uint64_t>
{
 public:
  // The 64 bit of the atomic Futex<uint64_t>::m_word have the following meaning:
  //
  // |______________________________64_bit_word_____________________________|
  // |____________31 msb____________|_spin_|__32 least significant bits_____|
  //  [         nwaiters            ]   [S] [           ntokens             ]
  //                                 <----------nwaiters_shift------------->  = 33
  //  0000000000000000000000000000001    0  00000000000000000000000000000000  = 0x200000000 = one_waiter
  //  0000000000000000000000000000000    1  00000000000000000000000000000000  = 0x100000000 = spinner_mask
  //  0000000000000000000000000000000    0  11111111111111111111111111111111  =  0xffffffff = tokens_mask
  static constexpr int nwaiters_shift = 33;
  static constexpr uint64_t one_waiter = (uint64_t)1 << nwaiters_shift;
  static constexpr uint64_t spinner_mask = one_waiter >> 1;
  static constexpr uint64_t tokens_mask = spinner_mask - 1;
  static_assert(utils::log2(tokens_mask) < 32, "Must fit in a futex (32 bit).");

  // Construct a SpinSemaphore with no waiters, no spinner and no tokens.
  SpinSemaphore() : aithreadsafe::Futex<uint64_t>(0) { }

  // Add n tokens to the semaphore.
  //
  // If there is no spinner but there are blocking threads (in wait()) then (at most) n threads
  // are woken up using a single system call. Each of those threads then will try to grab a token
  // before returning from wait(). If a thread fails to grab a token (because the tokens were
  // already grabbed by other threads that called wait() since) then they remain in wait() as
  // if not woken up.
  //
  // However, if there is a spinner thread then only that thread is woken up (without system call).
  // That thread will then wake up the additional threads, if any, before returning from wait().
  void post(uint32_t n = 1) noexcept
  {
    DoutEntering(dc::notice, "SpinSemaphore::post(" << n << ")");
    // Don't call post with n == 0.
    ASSERT(n >= 1);
    // Add n tokens.
    uint64_t const prev_word = m_word.fetch_add(n, std::memory_order_relaxed);
    uint32_t const prev_ntokens = prev_word & tokens_mask;
    bool const have_spinner = prev_word & spinner_mask;

#if CW_DEBUG
    Dout(dc::notice, "tokens " << prev_ntokens << " --> " << (prev_ntokens + n));
    // Check for possible overflow.
    ASSERT(prev_ntokens + n <= tokens_mask);
#endif

    // We avoid doing a syscall here, so if we have a spinner we're done.
    if (!have_spinner)
    {
      // No spinner was woken up, so we must do a syscall.
      //
      // Are there potential waiters that need to be woken up?
      uint32_t nwaiters = prev_word >> nwaiters_shift;
      if (nwaiters > prev_ntokens)
      {
        Dout(dc::notice, "Calling Futex<uint64_t>::wake(" << n << ") because nwaiters > prev_tokens (" << nwaiters << " > " << prev_tokens << ").");
        DEBUG_ONLY(uint32_t woken_up =) Futex<uint64_t>::wake(n);
        Dout(dc::notice, "Woke up " << woken_up << " threads.");
        ASSERT(woken_up <= n);
      }
    }
  }

  // Try to remove a token from the semaphore.
  //
  // Returns a recently read value of m_word.
  // m_word was not changed by this function when (word & tokens_mask) == 0,
  // otherwise the number of tokens were decremented by one and the returned
  // is the value of m_word immediately before this decrement.
  uint64_t fast_try_wait() noexcept
  {
    uint64_t word = m_word.load(std::memory_order_relaxed);
    do
    {
      uint64_t ntokens = word & tokens_mask;
      Dout(dc::notice, "tokens = " << ntokens << "; waiters = " << (word >> nwaiters_shift) << "; spinner = " << (word & spinner_mask) ? "yes" : "no");
      // Are there any tokens to grab?
      if (ntokens == 0)
        return word;           // No debug output needed: if the above line prints tokens = 0 then this return is implied.
      // We seem to have a token, try to grab it.
    }
    while (!m_word.compare_exchange_weak(word, word - 1, std::memory_order_acquire));
    // Token successfully grabbed.
    Dout(dc::notice, "Success, now " << ((word & tokens_mask) - 1) << " tokens left.");
    return word;
  }

  // Same as wait() but should only be called when (at least, very recently)
  // there are no tokens so that we are very likely to go to sleep.
  // word must be this very recently read value of m_word (so, (word & tokens_mask) should be 0).
  void slow_wait(uint64_t word) noexcept
  {
    // Calling slow_wait implies we have (had) no tokens, hence this should be true.
    // Don't call slow_wait unless this is true.
    ASSERT((word & tokens_mask) == 0);

    bool already_had_spinner;
    uint64_t ntokens;
    uint64_t new_word;
    do
    {
      already_had_spinner = (word & spinner_mask);
      ntokens = word & tokens_mask;
      new_word = !ntokens ? (word + one_waiter)         // We are (likely) going to block: add one to the number of waiters.
                            | spinner_mask              // Also, if there isn't already a spinner, grab the spinner bit.
                          : word - 1;                   // Someone added a new token before we even could go to sleep. Try to grab it!
    }
    while (!m_word.compare_exchange_weak(word, new_word, std::memory_order_seq_cst)); // FIXME: memory order.
    bool we_are_spinner = false;

    // Wait for a token to be available. Retry until we can grab one.
    for (;;)
    {
      if (AI_UNLIKELY(ntokens > 0))
      {
        // There were new tokens and we managed to grab one.
        Dout(dc::notice, "Successfully obtained a token. Now " << (ntokens - 1) << " tokens and " << ((word >> nwaiters_shift) - 1) << " waiters left.");
        return;
      }

      // At this point, !already_had_spinner means that we grabbed the spinner bit.
      we_are_spinner |= !already_had_spinner;

      // If there is no token available, block until a new token was added.

      if (!we_are_spinner)
      {
        // Already had a spinner, go to sleep.
        //
        // As of kernel 2.6.22 FUTEX_WAIT only returns with -1 when the syscall was
        // interrupted by a signal. In that case errno should be set to EINTR.
        // [Linux kernels before 2.6.22 could also return EINTR upon a spurious wakeup,
        // in which case it is also OK to just reenter wait() again.]
        [[maybe_unused]] int res;
        while ((res = Futex<uint64_t>::wait(ntokens)) == -1 && errno != EAGAIN)
          ;
        // EAGAIN happens when the number of tokens was changed in the meantime.
        // We (supuriously?) woke up or failed to go to sleep because the number of tokens changed.
        // It is therefore not sure that there is a token for us. Refresh word and try again.
        word = m_word.load(std::memory_order_relaxed);
        Dout(dc::notice(res == 0), "Woke up! tokens = " << (word & tokens_mask) << "; waiters = " << (word >> nwaiters_shift));
        // We woke up, try to again to get a token.
        do
        {
          already_had_spinner = (word & spinner_mask);
          ntokens = word & tokens_mask;
          Dout(dc::notice, "Seeing " << ntokens << " tokens and " << (word >> nwaiters_shift) << " waiters.");
          new_word = !ntokens ? word | spinner_mask
                              : word - one_waiter - 1;    // (Try to) atomically grab a token and stop being a waiter.
          // There is no need to do the CAS below when it would be a nop.
          if (!ntokens && already_had_spinner)
            break;
        }
        while (!m_word.compare_exchange_weak(word, new_word, std::memory_order_seq_cst)); // FIXME: memory order.
        // If ntokens > 0 here then we successfully grabbed one, otherwise
        // if already_had_spinner is false then we successfully became the spinner ourselves.
        // Go to the top of the loop to handle both situations...
      }
      else
      {
        do
        {
          // We are the spinner. Spin instead of going to sleep.
          do { cpu_relax(); } while (((word = m_word.load(std::memory_order_relaxed)) & tokens_mask) == 0);

          // New tokens were added.
          ntokens = word & tokens_mask;

          // Before we execute the following CAS new idle threads can decrement ntokens,
          // and because they subsequently continue running instead of going to sleep we'd
          // have to wake up less threads.
          //
          // It is also possible that ntokens is incremented because post() is called
          // while we are here; this then is still our reponsibility: as long as the
          // spinner bit is set we are responsible for waking up m_word & tokens_mask threads.
          //
          // If ntokens is reduced to 0 before we execute the following line then we must
          // continue to be the spinner.

          // Try to grab a token for ourselves, reset being the spinner and decrement the number
          // of waiters atomically, for as long as there are new tokens available.
          do
          {
            new_word = word - 1 - spinner_mask - one_waiter;
          }
          while (!m_word.compare_exchange_weak(word, new_word, std::memory_order_seq_cst) &&    // FIXME: memory order.
                 (ntokens = (word & tokens_mask)) > 0);
        }
        while (ntokens == 0);
        // We must wake up ntokens - 1 threads.
        // Are there potential waiters that need to be woken up?
        uint32_t nwaiters = word >> nwaiters_shift;
        if (nwaiters > 0 && ntokens > 1)
        {
          Dout(dc::notice, "Calling Futex<uint64_t>::wake(" << (ntokens - 1) << ") because there were waiters (" << nwaiters << ").");
          DEBUG_ONLY(uint32_t woken_up =) Futex<uint64_t>::wake(ntokens - 1);
          Dout(dc::notice, "Woke up " << woken_up << " threads.");
          ASSERT(woken_up <= ntokens);
        }
        return;
      }
    }
  }

  // Block until a token can be grabbed.
  //
  // If no token is available then the thread will block until it manages to grab a new token (added with post(n) by another thread).
  void wait() noexcept
  {
    DoutEntering(dc::notice, "SpinSemaphore::wait()");
    uint64_t word = fast_try_wait();
    if ((word & tokens_mask) == 0)
      slow_wait(word);
  }

  bool try_wait() noexcept
  {
    DoutEntering(dc::notice, "SpinSemaphore::try_wait()");
    return (fast_try_wait() & tokens_mask);
  }
};

} // namespace threadpool

class SpinSemaphore : public threadpool::SpinSemaphore
{
 public:
  uint64_t debug_word() const
  {
    return m_word.load(std::memory_order_relaxed);
  }

  void sanity_check() const
  {
    ASSERT(m_word == 0);
  }
};

SpinSemaphore sem;

// Count the total number of times that a thread was woken up.
std::atomic_int woken_up_count;
std::atomic_bool go;
std::atomic<unsigned long> slow, fast;
std::atomic<unsigned int> finished_sleepers;

using clock_type = std::chrono::steady_clock;

struct Point
{
  std::chrono::time_point<clock_type> m_time;
  unsigned long m_delay_loop;
  uint32_t m_tokens;

  Point(unsigned long dl, uint32_t tokens) : m_time(clock_type::now()), m_delay_loop(dl), m_tokens(tokens)
  {
    ASSERT(tokens < 40);
  }
};

using points_per_sleeper_thread_t = std::vector<Point>;

// Threads that try to go to sleep, increment a counter
// when they are woken up and then go back to sleep again.
void sleepers(points_per_sleeper_thread_t& points)
{
  while (!go)
    cpu_relax();

  for (int n = 0; n < number_of_times_to_wait_per_sleeper; ++n)
  {
    DoutEntering(dc::notice, "SpinSemaphore::wait()");
    uint64_t word = sem.fast_try_wait();
    unsigned long dl;
    if ((word & threadpool::SpinSemaphore::tokens_mask) == 0)
    {
      sem.slow_wait(word);
      ++slow;
      dl = delay_loop.load(std::memory_order_relaxed);
      while (dl >= post_amount && !delay_loop.compare_exchange_weak(dl, dl - post_amount, std::memory_order_relaxed))
        ;
    }
    else
    {
      dl = delay_loop.fetch_add(1, std::memory_order_relaxed);
      fast++;
    }
    woken_up_count++;
    points.emplace_back(dl, word & threadpool::SpinSemaphore::tokens_mask);
  }
  ++finished_sleepers;
}

void triggers()
{
//  benchmark::Stopwatch sw;

  while (!go)
    cpu_relax();

  // Wait till all sleepers threads sleep.
//  std::this_thread::sleep_for(std::chrono::milliseconds(10));

//  uint64_t diff_cycles_sum = 0;
//  int cnt = 0;

  for (int n = 0; n < number_of_times_to_post_per_trigger; ++n)
  {
    while ((sem.debug_word() & threadpool::SpinSemaphore::tokens_mask) > 8)
      cpu_relax();
//    sw.start();
    sem.post(post_amount);
//    sw.stop();

#if 0
    uint64_t dc = sw.diff_cycles();
    if (dc < 5 * 3612.05)
    {
      diff_cycles_sum += dc;
      ++cnt;
    }
#endif

#if 1
    unsigned long loop_size = delay_loop.load(std::memory_order_relaxed);
    for (unsigned long delay = 0; delay < loop_size; ++delay)
      asm volatile ("");
#else
    do
    {
      cpu_relax();
    }
    while ((sem.debug_word() >> threadpool::SpinSemaphore::nwaiters_shift) < number_of_sleeper_threads - finished_sleepers);
    for (unsigned long delay = 0; delay < 10000; ++delay)
      asm volatile ("");
#endif
  }

//  std::cout << "Ran on average for " << diff_cycles_sum / 3.612059050 / cnt <<
//    " nanoseconds (with " << (number_of_times_to_post_per_trigger - cnt) << " outliers)." << std::endl;
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Dout(dc::notice, "Entering main()...");

  std::vector<std::thread> sleeper_threads;
  std::array<points_per_sleeper_thread_t, number_of_sleeper_threads> plot_data;
  std::vector<std::thread> trigger_threads;

  std::string thread_name_base = "sleeper";
  char c = '1';
  for (int n = 0; n < number_of_sleeper_threads; ++n)
    sleeper_threads.emplace_back([n, thread_name = thread_name_base + c++, &plot_data](){ Debug(NAMESPACE_DEBUG::init_thread(thread_name)); sleepers(plot_data[n]); });
  thread_name_base = "trigger";
  c = '1';
  for (int n = 0; n < number_of_trigger_threads; ++n)
    trigger_threads.emplace_back([thread_name = thread_name_base + c++](){ Debug(NAMESPACE_DEBUG::init_thread(thread_name)); triggers(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto start = clock_type::now();
  go.store(1);

  for (auto& thread : trigger_threads)
    thread.join();
  for (auto& thread : sleeper_threads)
    thread.join();
  auto stop = clock_type::now();

  //Dout(dc::notice, "woken_up_count = " << woken_up_count);
  std::cout << "woken_up_count = " << woken_up_count << std::endl;
  ASSERT(woken_up_count == number_of_times_to_wait);
  std::cout << "delay_loop = " << delay_loop << std::endl;
  std::cout << "fast = " << fast << "; slow = " << slow << std::endl;

  // Draw a plot of the data.
  //
  // There are two sets of points, (time_point, delay_loop) and (t_time_point, tokens).
  //
  // The time_point runs from start to stop.
  constexpr int nbuckets = 3000;
  auto const width = (stop - start) / nbuckets;
  std::cout << "Bucket width = " << std::chrono::duration_cast<std::chrono::milliseconds>(width).count() << " ms." << std::endl;

  using bucket_t = unsigned int;
  auto get_bucket = [start, width](clock_type::time_point tp) -> bucket_t { return (tp - start + width / 2) / width; };

  std::array<std::pair<uint64_t, int>, nbuckets> delay_loop_sums;
  std::array<std::pair<uint64_t, int>, nbuckets> tokens_sums;
  delay_loop_sums.fill({0, 0});
  tokens_sums.fill({0, 0});

  for (auto& plot_data_per_thread : plot_data)
    for (auto point : plot_data_per_thread)
    {
      int bucket = get_bucket(point.m_time);
      if (bucket >= nbuckets)
      {
        std::cout << "* bucket overflow!" << std::endl;
        continue;
      }

      delay_loop_sums[bucket].first += point.m_delay_loop;
      delay_loop_sums[bucket].second++;

      tokens_sums[bucket].first += point.m_tokens;
      tokens_sums[bucket].second++;
    }

  std::array<uint64_t, nbuckets> delay_loop_avgs;
  std::array<uint32_t, nbuckets> tokens_avgs;
  uint64_t max_delay_loop_avg = 0;
  uint32_t max_tokens_avg = 0;
  for (int bucket = 0; bucket < nbuckets; ++bucket)
  {
    if (delay_loop_sums[bucket].second)
    {
      delay_loop_avgs[bucket] = delay_loop_sums[bucket].first / delay_loop_sums[bucket].second;
      max_delay_loop_avg = std::max(max_delay_loop_avg, delay_loop_avgs[bucket]);
    }
    else
      delay_loop_avgs[bucket] = 0;
    if (tokens_sums[bucket].second)
    {
      tokens_avgs[bucket] = tokens_sums[bucket].first / tokens_sums[bucket].second;
      max_tokens_avg = std::max(max_tokens_avg, tokens_avgs[bucket]);
    }
    else
      tokens_avgs[bucket] = 0;
  }

  double scale = 1.0 * max_delay_loop_avg / max_tokens_avg;

  std::cout << "max_tokens_avg = " << max_tokens_avg << "; max_delay_loop_avg = " << max_delay_loop_avg << "; scale = " << scale << std::endl;

  eda::PlotHistogram plot("delay loop size progression", "time (ms)", "delay loop size", std::chrono::duration_cast<std::chrono::milliseconds>(width).count());
  std::string const delay_loop_desc = "delay\\_loop";
  std::string const tokens_desc = "tokens";
  for (int bucket = 0; bucket < nbuckets; ++bucket)
  {
    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(bucket * width).count();
    plot.add_data_point(dt_ms, delay_loop_avgs[bucket], delay_loop_desc);
    plot.add_data_point(dt_ms, scale * tokens_avgs[bucket], tokens_desc);
  }
  plot.show();

  sem.sanity_check();

  Dout(dc::notice, "Leaving main()...");
}
