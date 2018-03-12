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

using clock_type = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clock_type>;
using duration = time_point::duration;
using ticks = time_point::rep;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
int constexpr loopsize = 250000; //10000000;

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

std::array<std::atomic_int, 3> expire_count;
std::array<std::atomic_int, 3> erase_count;
std::array<std::atomic_int, 3> cancelled;

void expire0() { expire_count[0]++; }
void expire1() { expire_count[1]++; }
void expire2() { expire_count[2]++; }

template<int implementation>
struct Timer;

template<int implementation>
struct Handle;

template<>
struct Handle<0>
{
  static std::multimap<time_point, Timer<0>*>::iterator const end;
  std::multimap<time_point, Timer<0>*>::iterator m_iter;

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_iter(end) { }

  // Construct a Handle that points to a given iterator.
  Handle(std::multimap<time_point, Timer<0>*>::iterator iter) : m_iter(iter) { }

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
struct Handle<1>
{
  Timer<1>* m_timer;

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_timer(nullptr) { }

  // Construct a Handle that points to a given timer.
  Handle(Timer<1>* timer) : m_timer(timer) { }

  bool is_running() const { return m_timer; }

  void set_not_running()
  {
    assert(m_timer);
    m_timer = nullptr;          // Mark this handle as being 'not running'.
  }
};

template<>
struct Handle<2>
{
  uint64_t m_sequence;
  interval_index m_interval;    // Interval index; -1 means: not running.

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_interval(-1) { }

  // Construct a Handle for a running timer with interval \a interval and sequence \sequence.
  constexpr Handle(interval_index interval, uint64_t sequence) : m_sequence(sequence), m_interval(interval) { }

  bool is_running() const { return m_interval >= 0; }
  void set_not_running() { m_interval = -1; }
};

static time_point last_time_point;

template<int implementation>
struct Timer
{
  Handle<implementation> m_handle;              // If m_handle.is_running() returns true then this timer is running
                                                //   and m_handle can be used to find the corresponding RunningTimer object.
  time_point m_expiration_point;                // The time at which we should expire (only valid when this is a running timer).
  std::function<void()> m_call_back;            // The callback function (only valid when this is a running timer).
  static int s_sequence_number;
  int const m_sequence_number;
  bool m_cancelled_1;                           // Only used when implementation == 1.

  Timer() : m_sequence_number(++s_sequence_number), m_cancelled_1(false) { }
  ~Timer() { stop(); }

  void start(interval_index interval, std::function<void()> call_back, int n);
  void stop();

  void expire()
  {
    assert(m_handle.is_running());
    assert(!m_cancelled_1);
    m_handle.set_not_running();
    if (implementation == 0)
      last_time_point = m_expiration_point;
    else if (last_time_point != m_expiration_point)
    {
      std::cout << "ERROR: Expiring a timer with m_expiration_point = " << m_expiration_point.time_since_epoch().count() << "; should be: " << last_time_point.time_since_epoch().count() << std::endl;
      assert(last_time_point == m_expiration_point);
    }
    std::cout << /*m_sequence_number <<*/ " : call_back()\t\t\t" << m_expiration_point.time_since_epoch().count() << "\n";
    m_call_back();
  }

  time_point get_expiration_point() const
  {
    return m_expiration_point;
  }
};

//static
template<int implementation>
int Timer<implementation>::s_sequence_number = 0;

template<class INTERVALS, int implementation>
class RunningTimers;

template<class INTERVALS>
class RunningTimers<INTERVALS, 0>
{
 private:
  std::multimap<time_point, Timer<0>*> m_map;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle<0> const& handle) const
  {
    assert(handle.is_running());
    return handle.m_iter == m_map.begin();
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle<0> push(int, Timer<0>* timer)
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
    Timer<0>* timer;
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

  std::multimap<time_point, Timer<0>*>::iterator end()
  {
    return m_map.end();
  }
};

template<class INTERVALS>
class RunningTimers<INTERVALS, 1>
{
 private:
   using data_t = std::pair<time_point, Timer<1>*>;
   struct Compare {
     bool operator()(data_t const& d1, data_t const& d2) { return d1.first > d2.first; }
   };
   std::priority_queue<data_t, std::vector<data_t>, Compare> m_pqueue;

 public:
  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle<1> const& handle) const
  {
    return handle.m_timer == m_pqueue.top().second;
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle<1> push(int, Timer<1>* timer)
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
    Timer<1>* timer;
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

struct RunningTimer
{
  Timer<2>* m_timer;                               // The underlaying Timer, or nullptr when the timer was cancelled.

  RunningTimer(Timer<2>* timer) : m_timer(timer) { }
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
  uint64_t push(Timer<2>* timer)
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

  void pop_cancelled_timer()
  {
    assert(!m_running_timers.empty());
    assert(!m_running_timers.front().m_timer);
    ++m_sequence_offset;
    m_running_timers.pop_front();
    ++erase_count[2];
    --cancelled[2];
  }

  bool cancel(uint64_t sequence)
  {
    size_t i = sequence - m_sequence_offset;
    assert(0 <= i && i < m_running_timers.size());
    m_running_timers[i].m_timer = nullptr;
    return i == 0;
  }

  bool front_is_cancelled() const
  {
    return m_running_timers.front().m_timer == nullptr;
  }

  // Return the expiration point for the related interval that will expire next.
  time_point next_expiration_point()
  {
    while (!m_running_timers.empty())
    {
      Timer<2>* timer = m_running_timers.front().m_timer;
      if (timer)
        return timer->get_expiration_point();
      pop_cancelled_timer();
    }
    return no_timer;
  }

  // Return true if are no running timers for the related interval.
  bool empty() const { return m_running_timers.empty(); }

  // Only used for testing.
  size_t size() const { return m_running_timers.size(); }
  int cancelled_in_queue() const
  {
    int sz = 0;
    for (auto timer : m_running_timers)
      sz += timer.m_timer ? 0 : 1;
    return sz;
  }

  void print() const
  {
    std::cout << "[offset:" << m_sequence_offset << "] ";
    for (auto timer : m_running_timers)
    {
      Timer<2>* t = timer.m_timer;
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
// Assume INTERVALS::number is 6, and thus tree_size == 8,
// then the structure of the data in RunningTimers could look like:
//
// Tournament tree (tree index:tree index)
// m_tree:                                              1:4
//                                             /                  \
//                                  2:0                                     3:4
//                              /         \                             /         \
//                        4:0                 5:3                 6:4                 7:6
//                      /     \             /     \             /     \             /     \
// m_cache:           18  no_timer       102        55        10        60  no_timer  no_timer
// index of m_cache:   0         1         2         3         4         5         6         7
//
// Thus, m_tree[1] contains 4, m_tree[2] contains 0, m_tree[3] contains 4 and so on.
// Each time a parent contains the same interval (in) as one of its children and well
// such that m_cache[in] contains the smallest time_point value of the two.
// m_cache[in] contains a copy of the top of m_queues[in].
//
template<class INTERVALS>
class RunningTimers<INTERVALS, 2>
{
  static int constexpr tree_size = utils::nearest_power_of_two(INTERVALS::number);

 private:
  std::array<uint8_t, tree_size> m_tree;
  std::array<time_point, tree_size> m_cache;
  std::array<Queue, INTERVALS::number> m_queues;

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

  static int constexpr interval_to_parent_index(interval_index in)
  {
    return (in + tree_size) / 2;
  }

  static int constexpr sibling_of(int index)
  {
    return index ^ 1;
  }

  void decrease_cache(interval_index interval, time_point tp)
  {
    std::cout << "Calling decrease_cache(" << interval << ", " << tp.time_since_epoch().count() << ")" << std::endl;
    assert(tp <= m_cache[interval]);
    m_cache[interval] = tp;                             // Replace no_timer with tp.
    // We just put a SMALLER value in the cache at position interval than what there was before.
    // Therefore all we have to do is overwrite parents with our interval until the time_point
    // value of the parent is less than tp.
    int parent_ti = interval_to_parent_index(interval); // Let 'parent_ti' be the index of the parent node in the tree above 'interval'.
    while (tp <= m_cache[m_tree[parent_ti]])            // m_tree[parent_ti] is the content of that node. m_cache[m_tree[parent_ti]] is the value.
    {
      m_tree[parent_ti] = interval;                     // Update that tree node.
      if (parent_ti == 1)                               // If this was the top-most node in the tree then we're done.
        break;
      parent_ti = parent_of(parent_ti);                 // Set 'i' to be the index of the parent node in the tree above 'i'.
    }
  }

  void increase_cache(interval_index interval, time_point tp)
  {
    std::cout << "Calling increase_cache(" << interval << ", " << tp.time_since_epoch().count() << ")" << std::endl;
    assert(tp >= m_cache[interval]);
    m_cache[interval] = tp;

    int parent_ti = interval_to_parent_index(interval); // Let 'parent_ti' be the index of the parent node in the tree above 'interval'.

    interval_index in = interval;                       // Let 'in' be the interval whose value is changed with respect to m_tree[parent_ti].
    interval_index si = in ^ 1;                         // Let 'si' be the interval of the current sibling of in.
    for(;;)
    {
      time_point sv = m_cache[si];
      if (tp > sv)
      {
        if (m_tree[parent_ti] == si)
          break;
        tp = sv;
        in = si;
      }
      m_tree[parent_ti] = in;                           // Update the tree.
      if (parent_ti == 1)                               // If this was the top-most node in the tree then we're done.
        break;
      si = m_tree[sibling_of(parent_ti)];               // Update the sibling interval.
      parent_ti = parent_of(parent_ti);                 // Set 'parent_ti' to be the index of the parent node in the tree above 'parent_ti'.
    }
  }

 public:
  RunningTimers();

  // For debugging. Expire the next timer.
  void expire_next();

  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle<2> const& handle) const
  {
    return m_tree[1] == handle.m_interval && m_queues[handle.m_interval].is_current(handle.m_sequence);
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle<2> push(interval_index interval, Timer<2>* timer)
  {
    assert(0 <= interval && interval < INTERVALS::number);
    bool empty = m_queues[interval].empty();
    uint64_t sequence = m_queues[interval].push(timer);
    if (empty)
      decrease_cache(interval, timer->get_expiration_point());
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

  int cancelled_in_queue() const
  {
    int sz = 0;
    for (auto&& queue : m_queues)
      sz += queue.cancelled_in_queue();
    return sz;
  }

  bool cancel(Handle<2> const handle)
  {
    assert(handle.is_running());
    // Cancel the timer associated with handle.
    Queue& queue{m_queues[handle.m_interval]};
    if (!queue.cancel(handle.m_sequence))       // Not the current timer for this interval?
      return false;                             // Then not the current timer.
    // The cancelled timer is at the front of the queue. Remove it, and any subsequent cancelled timers.
    do
    {
      queue.pop_cancelled_timer();
      if (queue.empty())
      {
        increase_cache(handle.m_interval, no_timer);// There are no timers left with this interval.
        break;
      }
    }
    while (queue.front_is_cancelled());             // Is the next timer also cancelled?
    // Return true if the cancelled timer is the currently running timer.
    return m_tree[1] == handle.m_interval;
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
};

template<class INTERVALS>
RunningTimers<INTERVALS, 2>::RunningTimers()
{
  for (interval_index interval = 0; interval < tree_size; ++interval)
  {
    m_cache[interval] = no_timer;
    int parent_ti = interval_to_parent_index(interval);
    m_tree[parent_ti] = interval & ~1;
  }
  for (int index = tree_size / 2 - 1; index > 0; --index)
  {
    m_tree[index] = m_tree[left_child_of(index)];
  }
}

std::mutex running_timers_mutex;
RunningTimers<Intervals, 0> running_timers0;
RunningTimers<Intervals, 1> running_timers1;
RunningTimers<Intervals, 2> running_timers2;

template<class INTERVALS>
void RunningTimers<INTERVALS, 2>::expire_next()
{
  std::cout << "Calling expire_next()" << std::endl;
  int const interval = m_tree[1];                             // The interval of the timer that will expire next.
  std::cout << "  m_tree[1] = " << interval << '\n';
  Queue& queue{m_queues[interval]};
  assert(!queue.empty());
  RunningTimer timer{queue.pop()};

  // Execute the algorithm for cache value becoming greater.
  increase_cache(interval, queue.next_expiration_point());
  running_timers2.print();

  std::cout << "  calling expire on timer [" << timer.m_timer->m_sequence_number << "]" << std::endl;
  timer.m_timer->expire();
  running_timers2.print();
}

// I'm assuming that end() doesn't invalidate, ever.
//static
std::multimap<time_point, Timer<0>*>::iterator const Handle<0>::end{running_timers0.end()};

void update_running_timer()
{
  // This is really only called 2 to 10 times in the very beginning.
  //std::cout << "Calling update_running_timer()\n";
}

template<>
void Timer<0>::start(interval_index interval, std::function<void()> call_back, int n)
{
  std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers0.push(interval, this);

  std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers0.is_current(m_handle))
    update_running_timer();
}

template<>
void Timer<1>::start(interval_index interval, std::function<void()> call_back, int n)
{
  std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers1.push(interval, this);

  m_cancelled_1 = false;
  std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers1.is_current(m_handle))
    update_running_timer();
}

template<>
void Timer<2>::start(interval_index interval, std::function<void()> call_back, int n)
{
  std::cout << "Calling Timer::start(interval = " << interval << ", ..., n = " << n << ") with this = [" << m_sequence_number << "]" << std::endl;
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers2.push(interval, this);

  std::cout << "  expires at " << m_expiration_point.time_since_epoch().count() << std::endl;
  if (running_timers2.is_current(m_handle))
    update_running_timer();
  running_timers2.print();
}

template<>
void Timer<0>::stop()
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
  else
    std::cout << "NOT running!\n";
}

template<>
void Timer<1>::stop()
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
  else
    std::cout << "NOT running!\n";
}

template<>
void Timer<2>::stop()
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
  else
    std::cout << "NOT running!\n";
  running_timers2.print();
}

int extra_timers{0};
template<int implementation>
std::vector<Timer<implementation>> timers;
template<>
std::vector<Timer<0>> timers<0>;
template<>
std::vector<Timer<1>> timers<1>;
template<>
std::vector<Timer<2>> timers<2>;

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
    Timer<0>& timer0(timers<0>[nt]);
    Timer<1>& timer1(timers<1>[nt]);
    Timer<2>& timer2(timers<2>[nt]);
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
  std::cout << "Running timers: " << (running_timers2.size() - cancelled[2]) << '\n';
  // For the remainder we wish to keep the number of running timers at around 100,000.
  // Therefore on average we should remove 1.5 timers per loop, the same amount that we add.
  // During this loop the ratio x/y goes to 1. Therefore each call to expire_next() removes
  // approximately 1.5 timers per call and we need to call it once per loop.
  double constexpr fraction = 0.012;  // Fine tuning. Call expire_next 1.20% times more often.
  int m = 131215 * fraction;
  for (int n = 131215; n < loopsize; ++n)
  {
    Timer<0>& timer0(timers<0>[nt]);
    Timer<1>& timer1(timers<1>[nt]);
    Timer<2>& timer2(timers<2>[nt]);
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
  std::cout << "Running timers: " << (running_timers2.size() - cancelled[2]) << '\n';
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Running: " << (running_timers0.size() - cancelled[0]) << '\n';
  std::cout << "Running: " << (running_timers1.size() - cancelled[1]) << '\n';
  std::cout << "Running: " << (running_timers2.size() - cancelled[2]) << '\n';
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
  int sum2 = running_timers2.size() + expire_count[2] + erase_count[2];
  std::cout << "running_timers.size() = " << running_timers0.size() << "; cancelled = " << cancelled[0] << '\n';
  std::cout << "running_timers.size() = " << running_timers1.size() << "; cancelled = " << cancelled[1] << '\n';
  std::cout << "running_timers.size() = " << running_timers2.size() << "; cancelled = " << cancelled[2] << '\n';
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
      "; still running timers: " << (running_timers2.size() - cancelled[2]) <<
      "; cancelled timers still in queue: " << cancelled[2] <<
      "; cancelled timers removed from queue = " << erase_count[2] <<
      "; expired timers = " << expire_count[2] << std::endl;
  assert(sum0 == loopsize + extra_timers);
  assert(sum1 == loopsize + extra_timers);
  assert(sum2 == loopsize + extra_timers);
  assert(cancelled[2] == running_timers2.cancelled_in_queue());
}
