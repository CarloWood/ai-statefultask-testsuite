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

// 0: multimap
// 1: priority_queue
// 2: My own design
#define IMPLEMENTATION 0

using clock_type = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clock_type>;
using duration = time_point::duration;
using ticks = time_point::rep;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
int constexpr loopsize = 10000000;

// In order to be reproducable, invent out own 'now()' function.
// Since we're aiming at adding loopsize in 1 second, we need
// to add 1,000,000,000 nanoseconds when n == loopsize.
time_point constexpr now(int n) { return time_point(duration{1520039479404233206L + n * 100L}); }

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
    m_iter->second = nullptr;
    m_iter = end;
  }
};
#elif IMPLEMENTATION == 1
#elif IMPLEMENTATION == 2
struct Handle
{
  uint64_t m_sequence;
  int m_interval;               // Interval index; 0 means: not running.

  // Default constructor. Construct a handle for a "not running timer".
  Handle() : m_interval(0) { }
  // Construct a Handle for a running timer with interval \a interval and sequence \sequence.
  Handle(int interval, uint64_t sequence) : m_sequence(sequence), m_interval(interval) { }
  bool is_running() const { return m_interval; }
  void set_not_running()
  {
    //m_handle->second.m_timer = nullptr;
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

  Timer() { }
  ~Timer() { stop(); }

  void start(interval_index interval, std::function<void()> call_back, int n);
  void stop();

  void expire()
  {
    assert(m_handle.is_running());
    m_handle.set_not_running();
    m_call_back();
  }

  time_point get_expiration_point() const
  {
    return m_expiration_point;
  }
};

struct RunningTimer
{
  Timer* m_timer;                               // The underlaying Timer, or nullptr when the timer was cancelled.

  RunningTimer(Timer* timer) : m_timer(timer) { }
};

class Queue
{
 private:
  uint64_t m_sequence_offset;                   // The number of timers that were popped from m_running_timers, minus 1.
  std::queue<RunningTimer> m_running_timers;    // All running timers for the related interval.
  int m_index;                                  // Index into RunningTimers<>::m_heap for the corresponding QueueNode.

 public:
  // Construct an empty queue associated with QueueNode \a index.
  Queue(int index) : m_sequence_offset(-1), m_index(index) { }

  // Return true if \a sequence is the value returned by a call to push() for
  // a timer that is now at the bottom (will be returned by pop()).
  bool is_current(uint64_t sequence) const { return sequence == m_sequence_offset; }

  // Add \a timer to the end of the queue. Returns an ever increasing sequence number.
  // The first sequence number returned is 0, then 1, 2, 3, ... etc.
  uint64_t push(Timer* timer)
  {
    m_running_timers.emplace(timer);
    return m_running_timers.size() + m_sequence_offset;
  }

  // Remove one timer from the front of the queue and return it.
  RunningTimer pop()
  {
    assert(!m_running_timers.empty());
    RunningTimer running_timer = m_running_timers.front();
    ++m_sequence_offset;
    m_running_timers.pop();
    return running_timer;
  }

  // Return the running timer for the related interval that will expire next.
  RunningTimer& front() { return m_running_timers.front(); }

  // Return true if are no running timers for the related interval.
  bool empty() const { return m_running_timers.empty(); }

  // Return the current index of this queue.
  int index() const { return m_index; }

  // Only used for testing.
  size_t size() const { return m_running_timers.size(); }
};

struct QueueNode
{
  union Value
  {
    time_point m_expiration_point;
    long m_index;

    Value() : m_index(0) { }    // FIXME: as soon as I know what m_index should be.
  };

  Value m_value;                // The value at which the current running timer for the corresponding interval should expire.
  Queue* m_queue;               // Pointer to the corresponding Queue.

  RunningTimer pop()
  {
    RunningTimer running_timer = m_queue->pop();
    if (m_queue->empty())
      m_value.m_index = 0;
    else
      // Keep a local copy of the expiration point of the current timer.
      m_value.m_expiration_point = m_queue->front().m_timer->get_expiration_point();
    return running_timer;
  }

  // Return true if \a sequence corresponds to the timer, of the to this QueueNode
  // corresponding interval, that is the first to expire next.
  bool is_current(uint64_t sequence) const { return m_queue->is_current(sequence); }

  time_point get_expiration_point() const
  {
    assert(m_value.m_index != 0);
    return m_value.m_expiration_point;
  }

  // Only used for testing.
  size_t size() const { return m_queue->size(); }
};

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
#elif IMPLEMENTATION == 2
template<class INTERVALS>
class RunningTimers
{
 private:
  std::array<QueueNode, INTERVALS::number> m_heap;
  std::array<Queue, INTERVALS::number> m_queues;

 public:
  RunningTimers();

  // Return true if \a handle is the next timer to expire.
  bool is_current(Handle const& handle) const
  {
    return handle.m_interval == 1 && m_heap[1].is_current(handle.m_sequence);
  }

  // Add \a timer to the list of running timers, using \a interval as timeout.
  Handle push(int interval, Timer* timer)
  {
    uint64_t sequence = m_queues[interval].push(timer);
    if (m_queues[interval].is_current(sequence))
      balance(m_queues[interval].index());
    return {interval, sequence};
  }

  // Only for debug output.
  size_t size() const
  {
    size_t sz = 0;
    for (auto&& node : m_heap)
      sz += node.size();
    return sz;
  }

  // For debugging. Expire the next timer.
  void expire_next()
  {
    m_heap[1].pop().m_timer->expire();
    balance(1);
  }

 private:
  void balance(int index)
  {
  }
};

template <int... N> auto make_array(std::integer_sequence<int, N...> i) { return std::array<Queue, i.size()>{{N...}}; }

template<class INTERVALS>
RunningTimers<INTERVALS>::RunningTimers() : m_queues{make_array(std::make_integer_sequence<int, INTERVALS::number>())}
{
  for (int i = 0; i < INTERVALS::number; ++i)
    m_heap[i].m_queue = &m_queues[i];
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
  // Call stop() first.
  assert(!m_handle.is_running());
  m_expiration_point = /*clock_type::*/now(n) + Intervals::durations[interval];
  m_call_back = call_back;
  std::lock_guard<std::mutex> lk(running_timers_mutex);
  m_handle = running_timers.push(interval, this);
  if (running_timers.is_current(m_handle))
    update_running_timer();
}

void Timer::stop()
{
  if (m_handle.is_running())
  {
    bool update = running_timers.is_current(m_handle);
    m_handle.set_not_running();
    cancelled++;
    if (update)
      update_running_timer();
  }
}

int extra_timers{0};

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

  std::vector<Timer> timers(loopsize + extra_timers);
  size_t nt = 0;

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
  for (int n = 0; n < 131215; ++n)
  {
    Timer& timer(timers[nt++]);
    int interval = random_intervals[n];

    timer.start(interval, &expire, n);       // The actual benchmark: how many timers can we add per second?

    if (n % 2 == 0 && interval > 0)     // Half the time, cancel the timer before it expires.
    {
      timers[nt++].start(interval - 1, [&timer](){ /*"destruct" timer*/ timer.stop(); expire_count++; }, n);
      running_timers.expire_next();
    }
  }
  // For the remainder we wish to keep the number of running timers at around 100,000.
  // Therefore on average we should remove 1.5 timers per loop, the same amount that we add.
  // During this loop the ratio x/y goes to 1. Therefore each call to expire_next() removes
  // approximately 1.5 timers per call and we need to call it once per loop.
  double constexpr fraction = 0.00965;  // Fine tuning. Call expire_next 0.965% times more often.
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
  auto end = std::chrono::high_resolution_clock::now();
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
  std::cout << "loopsize (type X timers) = " << loopsize <<
      "; type Y timers: " << extra_timers <<
      "; still running timers: " << (running_timers.size() - cancelled) <<
      "; cancelled timers still in queue: " << cancelled <<
      "; cancelled timers removed from queue = " << erase_count <<
      "; expired timers = " << expire_count << std::endl;
  assert(sum == loopsize + extra_timers);
}
