#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <iomanip>
#include <queue>
#include <atomic>
#include <map>
#include <mutex>
#include <cassert>
#include <functional>
#include <array>
#include <queue>
#include "utils/is_power_of_two.h"
#include "utils/nearest_power_of_two.h"
#include "statefultask/RunningTimers.h"
#include "statefultask/TimerQueue.h"

namespace statefultask {

//static
Timer::time_point constexpr statefultask::Timer::none;

} // namespace statefultask

// 0: multimap
// 1: priority_queue
// 2: My own design

using Timer = statefultask::Timer;
using interval_t = Timer::interval_t;
using time_point = Timer::time_point;
using duration = time_point::duration;
using ticks = time_point::rep;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
int constexpr loopsize = 10000000;

// In order to be reproducable, invent out own 'now()' function.
// Since we're aiming at adding loopsize in 10 second, we need
// to add 10,000,000,000 nanoseconds when n == loopsize.
time_point constexpr now(int n) { return time_point(duration{/*1520039479404233206L +*/ 10000000000L / loopsize * n}); }

// Lets assume that a single program can use up to 64 different intervals (but no more).
interval_t constexpr max_interval_index = 38;

struct Intervals
{
  static int constexpr number = max_interval_index + 1;
  using array_t = std::array<duration, number>;
  static array_t constexpr durations = {
    { microseconds(100), microseconds(150), microseconds(200), microseconds(250), microseconds(500),

      milliseconds(1),    milliseconds(2),    milliseconds(3),    milliseconds(4),    milliseconds(5),    milliseconds(6),    milliseconds(8),
      milliseconds(10),   milliseconds(12),   milliseconds(15),   milliseconds(20),   milliseconds(25),   milliseconds(30),   milliseconds(50),
      milliseconds(100),  milliseconds(200),  milliseconds(500),
      milliseconds(1000), milliseconds(1200), milliseconds(1500), milliseconds(1800), milliseconds(2000), milliseconds(2200), milliseconds(2500),
                          milliseconds(5000), milliseconds(7000),

      seconds(3), seconds(4), seconds(5), seconds(6), seconds(7), seconds(8), seconds(9), seconds(10) }
  };
};

//static
std::array<duration, max_interval_index + 1> constexpr Intervals::durations;

std::array<std::atomic_int, 3> expire_count;
std::array<std::atomic_int, 3> erase_count;
std::array<std::atomic_int, 3> cancelled;

void expire0() { expire_count[0]++; }
void expire1() { expire_count[1]++; }
void expire2() { expire_count[2]++; }

template<int implementation>
struct TimerImpl;

template<int implementation>
struct TimerHandleImpl;

template<>
struct TimerHandleImpl<0>
{
  static std::multimap<time_point, TimerImpl<0>*>::iterator const end;
  std::multimap<time_point, TimerImpl<0>*>::iterator m_iter;

  // Default constructor. Construct a handle for a "not running timer".
  TimerHandleImpl() : m_iter(end) { }

  // Construct a Handle that points to a given iterator.
  TimerHandleImpl(std::multimap<time_point, TimerImpl<0>*>::iterator iter) : m_iter(iter) { }

  bool is_running() const { return m_iter != end; }

  void set_not_running()
  {
    assert(m_iter != end);
    assert(m_iter->second);
    m_iter->second = nullptr;   // Cancel timer.
    m_iter = end;               // Mark this handle as being 'not running'.
  }
};

template<>
struct TimerHandleImpl<1>
{
  TimerImpl<1>* m_timer;

  // Default constructor. Construct a handle for a "not running timer".
  TimerHandleImpl() : m_timer(nullptr) { }

  // Construct a Handle that points to a given timer.
  TimerHandleImpl(TimerImpl<1>* timer) : m_timer(timer) { }

  bool is_running() const { return m_timer; }

  void set_not_running()
  {
    assert(m_timer);
    m_timer = nullptr;          // Mark this handle as being 'not running'.
  }
};

template<>
struct TimerHandleImpl<2> : public Timer::Handle
{
  TimerHandleImpl() : Timer::Handle() { }
  constexpr TimerHandleImpl(interval_t interval, uint64_t sequence) : Timer::Handle(interval, sequence) { }
  TimerHandleImpl(Timer::Handle handle) : Timer::Handle(handle) { }
};

static time_point last_time_point;

template<int implementation>
struct TimerImpl;

template<>
struct TimerImpl<0>
{
  TimerHandleImpl<0> m_handle;                   // If m_handle.is_running() returns true then this timer is running
                                                 //   and m_handle can be used to find the corresponding Timer object.
  time_point m_expiration_point;          // The time at which we should expire (only valid when this is a running timer).
  std::function<void()> m_call_back;            // The callback function (only valid when this is a running timer).
  static int s_sequence_number;
  int const m_sequence_number;

  TimerImpl() : m_sequence_number(++s_sequence_number) { }
  ~TimerImpl() { stop(); }

  void start(interval_t interval, std::function<void()> call_back, int n);
  void stop();

  void expire()
  {
    assert(m_handle.is_running());
    m_handle.set_not_running();
    last_time_point = m_expiration_point;
    //std::cout << /*m_sequence_number <<*/ " : call_back()\t\t\t" << m_expiration_point.time_since_epoch().count() << "\n";
    m_call_back();
  }

  time_point get_expiration_point() const { return m_expiration_point; }
};

template<>
struct TimerImpl<1>
{
  TimerHandleImpl<1> m_handle;                  // If m_handle.is_running() returns true then this timer is running
                                                //   and m_handle can be used to find the corresponding Timer object.
  time_point m_expiration_point;         // The time at which we should expire (only valid when this is a running timer).
  std::function<void()> m_call_back;            // The callback function (only valid when this is a running timer).
  static int s_sequence_number;
  int const m_sequence_number;
  bool m_cancelled_1;

  TimerImpl() : m_sequence_number(++s_sequence_number), m_cancelled_1(false) { }
  ~TimerImpl() { stop(); }

  void start(interval_t interval, std::function<void()> call_back, int n);
  void stop();

  void expire()
  {
    assert(m_handle.is_running());
    assert(!m_cancelled_1);
    m_handle.set_not_running();
    if (last_time_point != m_expiration_point)
    {
      std::cout << "ERROR: Expiring a timer with m_expiration_point = " << m_expiration_point.time_since_epoch().count() << "; should be: " << last_time_point.time_since_epoch().count() << std::endl;
      assert(last_time_point == m_expiration_point);
    }
    //std::cout << /*m_sequence_number <<*/ " : call_back()\t\t\t" << m_expiration_point.time_since_epoch().count() << "\n";
    m_call_back();
  }

  time_point get_expiration_point() const { return m_expiration_point; }
};

template<>
struct TimerImpl<2> : public Timer
{
  static int s_sequence_number;
  int const m_sequence_number;
  TimerImpl() : m_sequence_number(++s_sequence_number) { }
  TimerImpl(Timer timer) : Timer(timer), m_sequence_number(++s_sequence_number) { }
};

//static
int TimerImpl<0>::s_sequence_number = 0;

//static
int TimerImpl<1>::s_sequence_number = 0;

//static
int TimerImpl<2>::s_sequence_number = 0;

void print(statefultask::TimerQueue const& queue)
{
  std::cout << "[offset:" << queue.debug_get_sequence_offset() << "] ";
  for (auto timer = queue.debug_begin(); timer != queue.debug_end(); ++timer)
  {
    if (*timer)
      std::cout << " [" << static_cast<TimerImpl<2>*>(*timer)->m_sequence_number << ']' << (*timer)->get_expiration_point().time_since_epoch().count();
    else
      std::cout << " <cancelled>";
  }
}

template<class INTERVALS, int implementation>
class RunningTimersImpl;

template<class INTERVALS>
class RunningTimersImpl<INTERVALS, 0>
{
 private:
  std::multimap<time_point, TimerImpl<0>*> m_map;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(TimerHandleImpl<0> const& handle) const
  {
    assert(handle.is_running());
    return handle.m_iter == m_map.begin();
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  TimerHandleImpl<0> push(int, TimerImpl<0>* timer)
  {
    return m_map.emplace(timer->get_expiration_point(), timer);
  }

  // Only for debug output.
  size_t size() const
  {
    return m_map.size();
  }

  // For debugging. Expire the next timer.
  void expire_next()
  {
    TimerImpl<0>* timer;
    do
    {
      auto b = m_map.begin();
      assert(b != m_map.end());
      timer = b->second;
      if (timer)
        timer->expire();
      else
      {
        ++erase_count[0];
        --cancelled[0];
      }
      m_map.erase(b);
    }
    while (!timer);
  }

  std::multimap<time_point, TimerImpl<0>*>::iterator end()
  {
    return m_map.end();
  }
};

template<class INTERVALS>
class RunningTimersImpl<INTERVALS, 1>
{
 private:
   using data_t = std::pair<time_point, TimerImpl<1>*>;
   struct Compare {
     bool operator()(data_t const& d1, data_t const& d2) { return d1.first > d2.first; }
   };
   std::priority_queue<data_t, std::vector<data_t>, Compare> m_pqueue;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(TimerHandleImpl<1> const& handle) const
  {
    return handle.m_timer == m_pqueue.top().second;
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  TimerHandleImpl<1> push(int, TimerImpl<1>* timer)
  {
    m_pqueue.emplace(timer->get_expiration_point(), timer);
    return timer;
  }

  // Only for debug output.
  size_t size() const
  {
    return m_pqueue.size();
  }

  // For debugging. Expire the next timer.
  void expire_next()
  {
    TimerImpl<1>* timer;
    do
    {
      assert(!m_pqueue.empty());
      data_t const& data = m_pqueue.top();
      timer = data.second;
      if (!timer->m_cancelled_1)
        timer->expire();
      else
      {
        ++erase_count[1];
        --cancelled[1];
      }
      m_pqueue.pop();
    }
    while (timer->m_cancelled_1);
  }
};

template<class INTERVALS>
class RunningTimersImpl<INTERVALS, 2> : public statefultask::RunningTimers<INTERVALS>
{
 public:
  RunningTimersImpl() { sanity_check(); }

  bool cancel(Timer::Handle const handle)
  {
    assert(handle.is_running());
    //std::cout << "Calling RunningTimers::cancel({[" << handle.m_sequence << "], in=" << handle. m_interval << "})\n";
    //std::cout << "Before:\n";
    //print();
    sanity_check();

    bool res;
    {
      statefultask::TimerQueue& queue{this->m_queues[handle.m_interval]};
      size_t size_diff = queue.debug_size();

      res = statefultask::RunningTimers<INTERVALS>::cancel(handle);

      size_diff -= queue.debug_size();
      erase_count[2] += size_diff;
      cancelled[2] -= size_diff;
    }

    //std::cout << "After:\n";
    //print();
    sanity_check();

    // Return true if the cancelled timer is the currently running timer.
    return res;
  }

  void print() const
  {
    std::cout << "Running timers:\n";
    int i = 0; // interval
    for (auto&& queue : this->m_queues)
    {
      if (!queue.empty())
      {
        std::cout << "  " << i << " :";
        queue.print();
        std::cout << '\n';
      }
      ++i;
    }
    i = 0;
    std::cout << "  cache:\n";
    for (auto&& tp : this->m_cache)
    {
      if (tp != Timer::none)
      {
        std::cout << "  " << i << " :" << tp.time_since_epoch().count() << '\n';
      }
      ++i;
    }
    std::cout << "  tree:\n";
    i = 2;
    int dist = 128;
    int in = dist / 2;
    std::cout << std::setfill(' ');
    for (int j = 1; j < statefultask::RunningTimers<INTERVALS>::tree_size; ++j)
    {
      uint8_t d = this->m_tree[j];
      std::cout << std::right << std::setw(in) << (int)d;
      in = dist;
      if (i - 1 == j)
      {
        std::cout << '\n';
        i <<= 1;
        dist >>= 1;
        in = dist / 2;
      }
    }
    for (i = 0; i < INTERVALS::number; ++i)
    {
      std::cout << std::setw(in) << (i / 10);
      in = dist;
    }
    std::cout << '\n';
    in = dist / 2;
    for (i = 0; i < INTERVALS::number; ++i)
    {
      std::cout << std::setw(in) << (i % 10);
      in = dist;
    }
    std::cout << std::endl;
  }

  void expire_next()
  {
    sanity_check();
    statefultask::RunningTimers<INTERVALS>::expire_next();
    sanity_check();
  }

  void sanity_check() const;
};

std::mutex running_timers_mutex;
RunningTimersImpl<Intervals, 0> running_timers0;
RunningTimersImpl<Intervals, 1> running_timers1;
RunningTimersImpl<Intervals, 2> running_timers2;

namespace statefultask {

template<class INTERVALS>
RunningTimers<INTERVALS>::RunningTimers()
{
  for (interval_t interval = 0; interval < tree_size; ++interval)
  {
    m_cache[interval] = Timer::none;
    int parent_ti = interval_to_parent_index(interval);
    m_tree[parent_ti] = interval & ~1;
  }
  for (int index = tree_size / 2 - 1; index > 0; --index)
  {
    m_tree[index] = m_tree[left_child_of(index)];
  }
}

template<class INTERVALS>
void RunningTimers<INTERVALS>::expire_next()
{
  //std::cout << "Calling expire_next()" << std::endl;
  int const interval = m_tree[1];                             // The interval of the timer that will expire next.
  //std::cout << "  m_tree[1] = " << interval << '\n';
  statefultask::TimerQueue& queue{m_queues[interval]};
  // During this test there will always be more timers.
  assert(!queue.debug_empty());
  Timer* timer;

  // Execute the algorithm for cache value becoming greater.
  {
    size_t size_diff = queue.debug_size() - 1; // -1 because pop() removes one timer that wasn't cancelled and is accounted for by the call to timer->expire() below.

    timer = queue.pop();
    increase_cache(interval, queue.next_expiration_point());

    size_diff -= queue.debug_size();
    erase_count[2] += size_diff;
    cancelled[2] -= size_diff;
    //running_timers2.print();
  }

  //std::cout << "  calling expire on timer [" << timer->m_sequence_number << "]" << std::endl;
  if (last_time_point != timer->get_expiration_point())
  {
    std::cout << "ERROR: Expiring a timer with m_expiration_point = " << timer->get_expiration_point().time_since_epoch().count() << "; should be: " << last_time_point.time_since_epoch().count() << std::endl;
    assert(last_time_point == timer->get_expiration_point());
  }
  timer->expire();
  //running_timers2.print();
}

} // namespace statefultask

template<class INTERVALS>
void RunningTimersImpl<INTERVALS, 2>::sanity_check() const
{
  //static int count;
  //std::cout << "sanity check #" << ++count << std::endl;

  // Every cache entry needs to have either no_timer in it when the corresponding queue is empty, or the first entry of that queue.
  for (interval_t interval = 0; interval < statefultask::RunningTimers<INTERVALS>::tree_size; ++interval)
  {
    if (interval >= INTERVALS::number)
      assert(this->m_cache[interval] == Timer::none);
    else
    {
      assert(this->m_queues[interval].debug_empty() || *this->m_queues[interval].debug_begin() != nullptr);
      assert(this->m_cache[interval] == this->m_queues[interval].next_expiration_point());
    }
  }
  for (interval_t interval = 0; interval < statefultask::RunningTimers<INTERVALS>::tree_size; interval += 2)
  {
    int ti = this->interval_to_parent_index(interval);
    assert((this->m_tree[ti] & ~1) == interval);
    assert(this->m_cache[this->m_tree[ti] ^ 1] >= this->m_cache[this->m_tree[ti]]);
  }
  for (int ti = statefultask::RunningTimers<INTERVALS>::tree_size - 1; ti > 1; --ti)
  {
    int pi = this->parent_of(ti);
    int si = this->sibling_of(ti);
    int pin = this->m_tree[pi];
    int in = this->m_tree[ti];
    int sin = this->m_tree[si];
    assert(pin == in || pin == sin);
    int oin = in + sin - pin;
    assert(this->m_cache[pin] <= this->m_cache[oin]);
  }
}

// I'm assuming that end() doesn't invalidate, ever.
//static
std::multimap<time_point, TimerImpl<0>*>::iterator const TimerHandleImpl<0>::end{running_timers0.end()};

void update_running_timer()
{
  // This is really only called 2 to 10 times in the very beginning.
  //std::cout << "Calling update_running_timer()\n";
}

void TimerImpl<0>::start(interval_t interval, std::function<void()> call_back, int n)
{
  //std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*Timer::clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers0.push(interval, this);

  //std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers0.is_current(m_handle))
    update_running_timer();
}

void TimerImpl<1>::start(interval_t interval, std::function<void()> call_back, int n)
{
  //std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*Timer::clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers1.push(interval, this);

  m_cancelled_1 = false;
  //std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers1.is_current(m_handle))
    update_running_timer();
}

void Timer::start(interval_t interval, std::function<void()> call_back, int n)
{
  //std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*Timer::clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  running_timers2.sanity_check();
  m_handle = running_timers2.push(interval, this);
  running_timers2.sanity_check();

  //std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers2.is_current(m_handle))
    update_running_timer();
  //running_timers2.print();
}

void TimerImpl<0>::stop()
{
  //std::cout << "Calling Timer::stop() with this = [" << m_sequence_number << "]" << std::endl;
  if (m_handle.is_running())
  {
    bool update = running_timers0.is_current(m_handle);
    m_handle.set_not_running();
    cancelled[0]++;
    if (update)
      update_running_timer();
  }
  //else
  //  std::cout << "NOT running!\n";
}

void TimerImpl<1>::stop()
{
  //std::cout << "Calling Timer::stop() with this = [" << m_sequence_number << "]" << std::endl;
  if (m_handle.is_running())
  {
    bool update;
    update = running_timers1.is_current(m_handle);
    assert(!m_cancelled_1);
    m_cancelled_1 = true;

    m_handle.set_not_running();
    cancelled[1]++;
    if (update)
      update_running_timer();
  }
  //else
  //  std::cout << "NOT running!\n";
}

void Timer::stop()
{
  //std::cout << "Calling Timer::stop() with this = [" << m_sequence_number << "]" << std::endl;
  if (m_handle.is_running())
  {
    bool update;
    update = running_timers2.cancel(m_handle);

    m_handle.set_not_running();
    cancelled[2]++;
    if (update)
      update_running_timer();
  }
  //else
  //  std::cout << "NOT running!\n";
  //running_timers2.print();
}

int extra_timers{0};
template<int implementation>
std::vector<TimerImpl<implementation>> timers;
template<>
std::vector<TimerImpl<0>> timers<0>;
template<>
std::vector<TimerImpl<1>> timers<1>;
template<>
std::vector<TimerImpl<2>> timers<2>;

void generate()
{
  std::mt19937 rng;
  rng.seed(958723985);
  std::uniform_int_distribution<ticks> dist(0, max_interval_index);

  std::cout << "Generating random numbers..." << std::endl;
  std::vector<interval_t> random_intervals(loopsize);
  for (int n = 0; n < loopsize; ++n)
  {
    random_intervals[n] = dist(rng);
    if (n % 2 == 0 && random_intervals[n] > 0)
      extra_timers++;
  }

  size_t nt = 0;
  timers<0>.resize(loopsize + extra_timers);
  timers<1>.resize(loopsize + extra_timers);
  timers<2>.resize(loopsize + extra_timers);

  std::cout << "Starting benchmark test..." << std::endl;
  auto start = std::chrono::high_resolution_clock::now();
  // This loop adds every 2 consecutive loops (one with even n followed by one with odd n),
  // 3 timers: 2 timers that call expire() (type X) and 1 timer that, when it expires, will cancel a
  // timer (that would have called expire()) before it expires (type Y).
  //
  // Also, every other loop, we call expire_next() which expires the timer in the queue
  // with the smallest expiration time_point.
  //
  // Let the number of running timers of type X be x and the number of running timers of type Y be y.
  // Assuming they are more or less randomly ordered, then the chance that the next timer to expire
  // is of type Y is y/(x+y).
  //
  // So, every other loop we either remove 1 timer of type X, or we remove a timer of type Y which
  // removes a timer of type X. Hence, every 2 loops we remove one timer of type X and, on average,
  // y/(x+y) timers of type Y.
  //
  // This means that effectively, every 2 loops we add one timer of type X and (1 - y/(x+y)) = x/(x+y)
  // timers of type Y. That should stablize after a while such that x/y = 1/(x/(x+y)) = (x+y)/x -->
  // (x/y)^2 - (x/y) - 1 = 0 --> x/y = (1 + sqrt(5)) / 2 = 1.618 (the golden ratio).
  //
  // So, on average we will be adding 1 + x/(x+y) = 1 + 1.618 y / (1.618 y + y) = 1.618 timers, every 2 loops.
  // In order to end up with a queue size of 100,000 we need to loop 123609 times, but because
  // we don't reach the golden ratio right away this has to be about 6% more (131215).
  int constexpr ne = std::min(131215, loopsize);
  for (int n = 0; n < ne; ++n)
  {
    TimerImpl<0>& timer0(timers<0>[nt]);
    TimerImpl<1>& timer1(timers<1>[nt]);
    TimerImpl<2>& timer2(timers<2>[nt]);
    ++nt;
    int interval = random_intervals[n];

    timer0.start(interval, &expire0, n);       // The actual benchmark: how many timers can we add per second?
    timer1.start(interval, &expire1, n);       // The actual benchmark: how many timers can we add per second?
    timer2.start(interval, &expire2, n);       // The actual benchmark: how many timers can we add per second?

    if (n % 2 == 0 && interval > 0)     // Half the time, cancel the timer before it expires.
    {
      timers<0>[nt].start(interval - 1, [&timer0](){ /*"destruct" timer*/ timer0.stop(); expire_count[0]++; }, n);
      timers<1>[nt].start(interval - 1, [&timer1](){ /*"destruct" timer*/ timer1.stop(); expire_count[1]++; }, n);
      timers<2>[nt].start(interval - 1, [&timer2](){ /*"destruct" timer*/ timer2.stop(); expire_count[2]++; }, n);
      ++nt;
      running_timers0.expire_next();
      running_timers1.expire_next();
      running_timers2.expire_next();
    }
  }
  std::cout << "Running timers: " << (running_timers0.size() - cancelled[0]) << '\n';
  std::cout << "Running timers: " << (running_timers1.size() - cancelled[1]) << '\n';
  std::cout << "Running timers: " << (running_timers2.debug_size() - cancelled[2]) << '\n';
  // For the remainder we wish to keep the number of running timers at around 100,000.
  // Therefore on average we should remove 1.5 timers per loop, the same amount that we add.
  // During this loop the ratio x/y goes to 1. Therefore each call to expire_next() removes
  // approximately 1.5 timers per call and we need to call it once per loop.
  double constexpr fraction = 0.012;  // Fine tuning. Call expire_next 1.20% times more often.
  int m = 131215 * fraction;
  for (int n = 131215; n < loopsize; ++n)
  {
    TimerImpl<0>& timer0(timers<0>[nt]);
    TimerImpl<1>& timer1(timers<1>[nt]);
    TimerImpl<2>& timer2(timers<2>[nt]);
    ++nt;
    int interval = random_intervals[n];

    timer0.start(interval, &expire0, n);       // The actual benchmark: how many timers can we add per second?
    timer1.start(interval, &expire1, n);       // The actual benchmark: how many timers can we add per second?
    timer2.start(interval, &expire2, n);       // The actual benchmark: how many timers can we add per second?

    if (n % 2 == 0 && interval > 0)     // Half the time, cancel the timer before it expires.
    {
      timers<0>[nt].start(interval - 1, [&timer0](){ /*"destruct" timer*/ timer0.stop(); expire_count[0]++; }, n);
      timers<1>[nt].start(interval - 1, [&timer1](){ /*"destruct" timer*/ timer1.stop(); expire_count[1]++; }, n);
      timers<2>[nt].start(interval - 1, [&timer2](){ /*"destruct" timer*/ timer2.stop(); expire_count[2]++; }, n);
      ++nt;
    }

    // Call expire_next() 1 + fraction times per loop.
    running_timers0.expire_next();
    running_timers1.expire_next();
    running_timers2.expire_next();
    if (m < n * fraction)
    {
      running_timers0.expire_next();
      running_timers1.expire_next();
      running_timers2.expire_next();
      ++m;
    }
  }
  std::cout << "Running timers: " << (running_timers0.size() - cancelled[0]) << '\n';
  std::cout << "Running timers: " << (running_timers1.size() - cancelled[1]) << '\n';
  std::cout << "Running timers: " << (running_timers2.debug_size() - cancelled[2]) << '\n';
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Running: " << (running_timers0.size() - cancelled[0]) << '\n';
  std::cout << "Running: " << (running_timers1.size() - cancelled[1]) << '\n';
  std::cout << "Running: " << (running_timers2.debug_size() - cancelled[2]) << '\n';
  std::cout << "Benchmark finished.\n";

  std::chrono::duration<double> diff = end - start;
  std::cout << "Total time: " << diff.count() << " seconds.\n";
  std::cout << "Called timer_start() " << std::fixed << std::setprecision(0) <<
      ((loopsize + extra_timers) / diff.count()) << " times per second on average.\n";
}

int main()
{
  std::thread generator(&generate);

  generator.join();

  int sum0 = running_timers0.size() + expire_count[0] + erase_count[0];
  int sum1 = running_timers1.size() + expire_count[1] + erase_count[1];
  int sum2 = running_timers2.debug_size() + expire_count[2] + erase_count[2];
  std::cout << "running_timers.size() = " << running_timers0.size() << "; cancelled = " << cancelled[0] << '\n';
  std::cout << "running_timers.size() = " << running_timers1.size() << "; cancelled = " << cancelled[1] << '\n';
  std::cout << "running_timers.size() = " << running_timers2.debug_size() << "; cancelled = " << cancelled[2] << '\n';
  std::cout << "loopsize (type X timers) = " << loopsize <<
      "; type Y timers: " << extra_timers <<
      "; still running timers: " << (running_timers0.size() - cancelled[0]) <<
      "; cancelled timers still in queue: " << cancelled[0] <<
      "; cancelled timers removed from queue = " << erase_count[0] <<
      "; expired timers = " << expire_count[0] << std::endl;
  std::cout << "loopsize (type X timers) = " << loopsize <<
      "; type Y timers: " << extra_timers <<
      "; still running timers: " << (running_timers1.size() - cancelled[1]) <<
      "; cancelled timers still in queue: " << cancelled[1] <<
      "; cancelled timers removed from queue = " << erase_count[1] <<
      "; expired timers = " << expire_count[1] << std::endl;
  std::cout << "loopsize (type X timers) = " << loopsize <<
      "; type Y timers: " << extra_timers <<
      "; still running timers: " << (running_timers2.debug_size() - cancelled[2]) <<
      "; cancelled timers still in queue: " << cancelled[2] <<
      "; cancelled timers removed from queue = " << erase_count[2] <<
      "; expired timers = " << expire_count[2] << std::endl;
  assert(sum0 == loopsize + extra_timers);
  assert(sum1 == loopsize + extra_timers);
  assert(sum2 == loopsize + extra_timers);
  assert(cancelled[2] == running_timers2.debug_cancelled_in_queue());
}
