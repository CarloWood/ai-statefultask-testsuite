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

// 0: multimap
// 1: priority_queue
// 2: My own design
#define IMPLEMENTATION 2

using clock_type = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clock_type>;
using duration = time_point::duration;
using ticks = time_point::rep;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
int constexpr loopsize = 10; //10000000;

// In order to be reproducable, invent out own 'now()' function.
// Since we're aiming at adding loopsize in 10 second, we need
// to add 10,000,000,000 nanoseconds when n == loopsize.
time_point constexpr now(int n) { return time_point(duration{/*1520039479404233206L +*/ 10000000000L / loopsize * n}); }

// Lets assume that a single program can use up to 64 different intervals (but no more).
using interval_index = int;
interval_index constexpr max_interval_index = 38;

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

std::atomic_int expire_count{0};
std::atomic_int erase_count{0};
std::atomic_int cancelled{0};

void expire()
{
  expire_count++;
}

#if IMPLEMENTATION == 0
struct Timer;
struct Handle
{
  static std::multimap<time_point, Timer*>::iterator const end;
  std::multimap<time_point, Timer*>::iterator m_iter;

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_iter(end) { }

  // Construct a Handle that points to a given iterator.
  Handle(std::multimap<time_point, Timer*>::iterator iter) : m_iter(iter) { }

  bool is_running() const { return m_iter != end; }

  void set_not_running()
  {
    assert(m_iter != end);
    assert(m_iter->second);
    m_iter->second = nullptr;   // Cancel timer.
    m_iter = end;               // Mark this handle as being 'not running'.
  }
};
#elif IMPLEMENTATION == 1
struct Timer;
struct Handle
{
  Timer* m_timer;

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_timer(nullptr) { }

  // Construct a Handle that points to a given timer.
  Handle(Timer* timer) : m_timer(timer) { }

  bool is_running() const { return m_timer; }

  void set_not_running()
  {
    assert(m_timer);
    m_timer = nullptr;          // Mark this handle as being 'not running'.
  }
};
#elif IMPLEMENTATION == 2
struct Handle
{
  uint64_t m_sequence;
  int m_interval;               // Interval index; 0 means: not running.

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_interval(0) { }

  // Construct a Handle for a running timer with interval \a interval and sequence \sequence.
  constexpr Handle(int interval, uint64_t sequence) : m_sequence(sequence), m_interval(interval) { }

  bool is_running() const { return m_interval; }

  void set_not_running()
  {
    m_interval = 0;
  }
};
#endif

struct Timer
{
  Handle m_handle;                              // If m_handle.is_running() returns true then this timer is running
                                                //   and m_handle can be used to find the corresponding RunningTimer object.
  time_point m_expiration_point;                // The time at which we should expire (only valid when this is a running timer).
  std::function<void()> m_call_back;            // The callback function (only valid when this is a running timer).
#if IMPLEMENTATION == 1
  bool m_cancelled;
#endif
  static int s_sequence_number;
  int const m_sequence_number;

  Timer() : m_sequence_number(++s_sequence_number) { }
  ~Timer() { stop(); }

  void start(interval_index interval, std::function<void()> call_back, int n);
  void stop();

  void expire()
  {
    assert(m_handle.is_running());
#if IMPLEMENTATION == 1
    assert(!m_cancelled);
#endif
    m_handle.set_not_running();
    std::cout << m_sequence_number << " : call_back()\t\t\t" << m_expiration_point.time_since_epoch().count() << "\n";
    m_call_back();
  }

  time_point get_expiration_point() const
  {
    return m_expiration_point;
  }
};

//static
int Timer::s_sequence_number = 0;

#if IMPLEMENTATION == 0
template<class INTERVALS>
class RunningTimers
{
 private:
  std::multimap<time_point, Timer*> m_map;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle const& handle) const
  {
    assert(handle.is_running());
    return handle.m_iter == m_map.begin();
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle push(int, Timer* timer)
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
    Timer* timer;
    do
    {
      auto b = m_map.begin();
      assert(b != m_map.end());
      timer = b->second;
      if (timer)
        timer->expire();
      else
      {
        ++erase_count;
        --cancelled;
      }
      m_map.erase(b);
    }
    while (!timer);
  }

  std::multimap<time_point, Timer*>::iterator end()
  {
    return m_map.end();
  }
};
#elif IMPLEMENTATION == 1
template<class INTERVALS>
class RunningTimers
{
 private:
   using data_t = std::pair<time_point, Timer*>;
   struct Compare {
     bool operator()(data_t const& d1, data_t const& d2) { return d1.first > d2.first; }
   };
   std::priority_queue<data_t, std::vector<data_t>, Compare> m_pqueue;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle const& handle) const
  {
    return handle.m_timer == m_pqueue.top().second;
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle push(int, Timer* timer)
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
    Timer* timer;
    do
    {
      assert(!m_pqueue.empty());
      data_t const& data = m_pqueue.top();
      timer = data.second;
      if (!timer->m_cancelled)
        timer->expire();
      else
      {
        ++erase_count;
        --cancelled;
      }
      m_pqueue.pop();
    }
    while (timer->m_cancelled);
  }
};
#elif IMPLEMENTATION == 2

struct RunningTimer
{
  Timer* m_timer;                               // The underlaying Timer, or nullptr when the timer was cancelled.

  RunningTimer(Timer* timer) : m_timer(timer) { }
};

// Use a value far in the future to represent 'no timer' (aka, a "timer" that will never expire).
time_point constexpr no_timer{duration(std::numeric_limits<ticks>::max())};

class Queue
{
 private:
  uint64_t m_sequence_offset;                   // The number of timers that were popped from m_running_timers, minus 1.
  std::deque<RunningTimer> m_running_timers;    // All running timers for the related interval.

 public:
  // Construct an empty queue.
  Queue() : m_sequence_offset(0) { }

  // Return true if \a sequence is the value returned by a call to push() for
  // a timer that is now at the bottom (will be returned by pop()).
  bool is_current(uint64_t sequence) const { return sequence == m_sequence_offset; }

  // Add \a timer to the end of the queue. Returns an ever increasing sequence number.
  // The first sequence number returned is 0, then 1, 2, 3, ... etc.
  uint64_t push(Timer* timer)
  {
    m_running_timers.emplace_back(timer);
    return m_running_timers.size() - 1 + m_sequence_offset;
  }

  // Remove one timer from the front of the queue and return it.
  RunningTimer pop()
  {
    assert(!m_running_timers.empty());
    RunningTimer running_timer{m_running_timers.front()};
    ++m_sequence_offset;
    m_running_timers.pop_front();
    return running_timer;
  }

  bool cancel(uint64_t sequence)
  {
    size_t i = sequence - m_sequence_offset;
    assert(0 <= i && i < m_running_timers.size());
    m_running_timers[i].m_timer = nullptr;
    return i == 0;
  }

  // Return the expiration point for the related interval that will expire next.
  time_point next_expiration_point() { return m_running_timers.empty() ? no_timer : m_running_timers.front().m_timer->get_expiration_point(); }

  // Return true if are no running timers for the related interval.
  bool empty() const { return m_running_timers.empty(); }

  // Only used for testing.
  size_t size() const { return m_running_timers.size(); }

  void print() const
  {
    std::cout << "[offset:" << m_sequence_offset << "] ";
    for (auto timer : m_running_timers)
    {
      Timer* t = timer.m_timer;
      if (t)
        std::cout << " [" << t->m_sequence_number << ']' << t->get_expiration_point().time_since_epoch().count();
      else
        std::cout << " <cancelled>";
    }
  }
};

// This is a tournament tree of queues of timers with the same interval.
//
// The advantage of using queues of timers with the same interval
// is that it is extremely cheap to insert a new timer with a
// given interval when there is already another timer with the
// same interval running. On top of that, it is likely that even
// a program that has a million timers running simultaneously
// only uses a handful of distinct intervals, so the size of
// the tree/heap shrinks dramatically.
//

template<class INTERVALS>
class RunningTimers
{
  static int constexpr tree_size = utils::nearest_power_of_two(INTERVALS::number);

 private:
  std::array<uint8_t, tree_size> m_tree;
  std::array<time_point, tree_size> m_cache;
  std::array<Queue, INTERVALS::number> m_queues;

  static int constexpr index_to_interval(int index)
  {
    return index - tree_size;
  }

  static int constexpr interval_to_index(int interval)
  {
    return interval + tree_size;
  }

  static int constexpr parent_of(int index)
  {
    return index / 2;
  }

  static int constexpr left_child_of(int index)
  {
    return index * 2;
  }

  static int constexpr right_child_of(int index)
  {
    return index * 2 + 1;
  }

 public:
  RunningTimers();
// Assume INTERVALS::number is 6, and thus tree_size == 8,
// then the structure of the data in RunningTimers could look like:
//
// Tournament tree (tree index:tree index)
// m_tree:                                              1:12
//                                             /                  \
//                                  2:8                                     3:12
//                              /         \                             /         \
//                        4:8                5:11                6:12                7:14
//                      /     \             /     \             /     \             /     \
// m_cache:           18  no_timer       102        55        10        60  no_timer  no_timer
// index of m_cache:   0         1         2         3         4         5         6         7
// corr. tree index:   8         9        10        11        12        13        14        15
//
// Thus, m_tree[1] contains 12, m_tree[2] contains 8, m_tree[3] contains 12 and so on.
// Each time a parent contains the same index as one of its children and well that
// index (i+tree_size) such that m_cache[i] contains the smaller time_point value.
// m_cache[i] contains a copy of the top of m_queues[i].
//

  // For debugging. Expire the next timer.
  void expire_next()
  {
    std::cout << "Calling expire_next()" << std::endl;
    int i = m_tree[1];
    int const interval = index_to_interval(i);
    std::cout << "  m_tree[1] = " << i << "; interval = " << interval << '\n';
    Queue& queue{m_queues[interval]};
    assert(!queue.empty());
    RunningTimer timer{queue.pop()};
    time_point tp = queue.next_expiration_point();
    m_cache[interval] = tp;
    // i = 12, tp = 55 / 65 / 15 / 20
    int p = i / 2;              // p = 6
    int ss = 4 * p + 1;         // ss = 12 + 13 = 25
    int s = ss - i;             // s = 13
    time_point stp = m_cache[index_to_interval(s)];
    while (tp <= stp)
    {
      tp = stp;
      m_tree[p] = s;
      if (p == 1)
        break;
      i = p;
      p = i / 2;              // p = 6
      ss = 4 * p + 1;         // ss = 12 + 13 = 25
      s = ss - i;             // s = 13
      stp = m_cache[index_to_interval(m_tree[s])];
    }
    std::cout << "  calling expire on timer [" << timer.m_timer->m_sequence_number << "]" << std::endl;
    timer.m_timer->expire();
  }

  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle const& handle) const
  {
    return index_to_interval(m_tree[1]) == handle.m_interval && m_queues[handle.m_interval].is_current(handle.m_sequence);
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle push(int interval, Timer* timer)
  {
    assert(0 <= interval && interval < INTERVALS::number);
    bool empty = m_queues[interval].empty();
    uint64_t sequence = m_queues[interval].push(timer);
    if (empty)
    {
      time_point new_tp = timer->get_expiration_point();
      m_cache[interval] = new_tp;       // Replace no_timer with new_tp.
      // We just put a SMALLER value in the cache at position interval than what there was before.
      // Therefore all we have to do is overwrite parents with our tree index until the time_point
      // value of the parent is less than or equal in value then new_tp.
      int const new_ti = interval_to_index(interval);   // The Tree Index of the new (smaller) value.
      int parent_ti = new_ti >> 1;
      while (parent_ti && new_tp < m_cache[index_to_interval(m_tree[parent_ti])])
      {
        m_tree[parent_ti] = new_ti;
        parent_ti >>= 1;
      }
    }
    return {interval, sequence};
  }

  // Only for debug output.
  size_t size() const
  {
    size_t sz = 0;
    for (auto&& queue : m_queues)
      sz += queue.size();
    return sz;
  }

  bool cancel(Handle handle)
  {
    bool is_current = m_queues[handle.m_interval].cancel(handle.m_sequence);
    if (is_current)
      m_cache[handle.m_interval] = no_timer;
    return is_current && index_to_interval(m_tree[1]) == handle.m_interval;
  }

  void print() const
  {
    std::cout << "Running timers:\n";
    int i = 0; // interval
    for (auto&& queue : m_queues)
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
    for (auto&& tp : m_cache)
    {
      if (tp != no_timer)
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
    for (int j = 1; j < tree_size; ++j)
    {
      uint8_t d = m_tree[j];
      std::cout << std::right << std::setw(in) << ((int)d - 64);
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
};

template<class INTERVALS>
RunningTimers<INTERVALS>::RunningTimers()
{
  for (int interval = 0; interval < tree_size; ++interval)
  {
    m_cache[interval] = no_timer;
    int index = interval_to_index(interval);
    int parent = parent_of(index);
    m_tree[parent] = left_child_of(parent);
  }
  for (int index = tree_size / 2 - 1; index > 0; --index)
  {
    m_tree[index] = m_tree[left_child_of(index)];
  }
}
#endif

std::mutex running_timers_mutex;
RunningTimers<Intervals> running_timers;

#if IMPLEMENTATION == 0
// I'm assuming that end() doesn't invalidate, ever.
//static
std::multimap<time_point, Timer*>::iterator const Handle::end{running_timers.end()};
#endif

void update_running_timer()
{
  // This is really only called 2 to 10 times in the very beginning.
  //std::cout << "Calling update_running_timer()\n";
}

void Timer::start(interval_index interval, std::function<void()> call_back, int n)
{
  std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers.push(interval, this);
#if IMPLEMENTATION == 1
  m_cancelled = false;
#endif
  std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers.is_current(m_handle))
    update_running_timer();
  running_timers.print();
}

void Timer::stop()
{
  std::cout << "Calling Timer::stop() with this = [" << m_sequence_number << "]" << std::endl;
  if (m_handle.is_running())
  {
#if IMPLEMENTATION != 2
    bool update = running_timers.is_current(m_handle);
#endif
#if IMPLEMENTATION == 1
    assert(!m_cancelled);
    m_cancelled = true;
#elif IMPLEMENTATION == 2
    bool update = running_timers.cancel(m_handle);
#endif
    m_handle.set_not_running();
    cancelled++;
    if (update)
      update_running_timer();
  }
  else
    std::cout << "NOT running!\n";
  running_timers.print();
}

int extra_timers{0};
std::vector<Timer> timers;

void generate()
{
  std::mt19937 rng;
  rng.seed(958723985);
  std::uniform_int_distribution<ticks> dist(0, max_interval_index);

  std::cout << "Generating random numbers..." << std::endl;
  std::vector<interval_index> random_intervals(loopsize);
  for (int n = 0; n < loopsize; ++n)
  {
    random_intervals[n] = dist(rng);
    if (n % 2 == 0 && random_intervals[n] > 0)
      extra_timers++;
  }

  size_t nt = 0;
  timers.resize(loopsize + extra_timers);

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
    Timer& timer(timers[nt++]);
    int interval = random_intervals[n];

    timer.start(interval, &expire, n);       // The actual benchmark: how many timers can we add per second?

    if (n % 2 == 0 && interval > 0)     // Half the time, cancel the timer before it expires.
    {
      timers[nt++].start(interval - 1, [&timer](){ /*"destruct" timer*/ /*timer.stop();*/ expire_count++; }, n);
      running_timers.expire_next();
    }
  }
  std::cout << "Running timers: " << (running_timers.size() - cancelled) << '\n';
  // For the remainder we wish to keep the number of running timers at around 100,000.
  // Therefore on average we should remove 1.5 timers per loop, the same amount that we add.
  // During this loop the ratio x/y goes to 1. Therefore each call to expire_next() removes
  // approximately 1.5 timers per call and we need to call it once per loop.
  double constexpr fraction = 0.012;  // Fine tuning. Call expire_next 1.20% times more often.
  int m = 131215 * fraction;
  for (int n = 131215; n < loopsize; ++n)
  {
    Timer& timer(timers[nt++]);
    int interval = random_intervals[n];

    timer.start(interval, &expire, n);       // The actual benchmark: how many timers can we add per second?

    if (n % 2 == 0 && interval > 0)     // Half the time, cancel the timer before it expires.
      timers[nt++].start(interval - 1, [&timer](){ /*"destruct" timer*/ timer.stop(); expire_count++; }, n);

    // Call expire_next() 1 + fraction times per loop.
    running_timers.expire_next();
    if (m < n * fraction)
    {
      running_timers.expire_next();
      ++m;
    }
  }
  std::cout << "Running timers: " << (running_timers.size() - cancelled) << '\n';
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Running: " << (running_timers.size() - cancelled) << '\n';
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

  int sum = running_timers.size() + expire_count + erase_count;
  std::cout << "running_timers.size() = " << running_timers.size() << "; cancelled = " << cancelled << '\n';
  std::cout << "loopsize (type X timers) = " << loopsize <<
      "; type Y timers: " << extra_timers <<
      "; still running timers: " << (running_timers.size() - cancelled) <<
      "; cancelled timers still in queue: " << cancelled <<
      "; cancelled timers removed from queue = " << erase_count <<
      "; expired timers = " << expire_count << std::endl;
  assert(sum == loopsize + extra_timers);
}
