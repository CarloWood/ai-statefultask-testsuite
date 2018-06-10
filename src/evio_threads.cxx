#include "sys.h"
#include "ev.h"
#include "debug.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <chrono>

class EventLoopThread
{
 private:
  std::thread m_event_thread;

  struct ev_loop* loop;
  std::mutex m_loop_mutex;
  std::condition_variable m_invoke_handled_cv;
  bool m_invoke_handled;

  ev_async m_async_w;
  bool m_running;

  std::mutex m_invoke_pending_mutex;
  std::condition_variable m_invoke_pending_cv;
  bool m_invoke_pending;

  static void acquire(EV_P);
  static void release(EV_P);
  static void main(EV_P);
  static void handle_invoke_pending(EV_P);
  static void async_cb(EV_P_ ev_async* w, int revents);

  void run();
  void handle_invoke_pending();

 public:
  EventLoopThread();
  ~EventLoopThread();

  bool running() const { return m_running; }

  void add(ev_timer& timeout_watcher);
  void add(ev_io& io_watcher);
  bool wait_for_events();
  void invoke_pending();
};

//static
void EventLoopThread::acquire(EV_P)
{
  static_cast<EventLoopThread*>(ev_userdata(EV_A))->m_loop_mutex.lock();
}

//static
void EventLoopThread::release(EV_P)
{
  static_cast<EventLoopThread*>(ev_userdata(EV_A))->m_loop_mutex.unlock();
}

//static
void EventLoopThread::handle_invoke_pending(EV_P)
{
  static_cast<EventLoopThread*>(ev_userdata(EV_A))->handle_invoke_pending();
}

//static
void EventLoopThread::main(EV_P)
{
  Debug(NAMESPACE_DEBUG::init_thread());
  Dout(dc::notice, "Event loop thread started.");
  static_cast<EventLoopThread*>(ev_userdata(EV_A))->run();
}

//static
void EventLoopThread::async_cb(EV_P_ ev_async*, int)
{
  // Just used for the side effects.
}

void EventLoopThread::run()
{
  std::lock_guard<std::mutex> lock(m_loop_mutex);
  m_running = true;
  Dout(dc::notice, "Calling ev_run(0)");
  ev_run(EV_A_ 0);
  Dout(dc::notice, "Returned from ev_run(0)");
  // Exit main thread loop.
  m_running = false;
  // Wake up main loop.
  m_invoke_pending = true;
  m_invoke_pending_cv.notify_one();
}

bool EventLoopThread::wait_for_events()
{
  std::unique_lock<std::mutex> lock(m_invoke_pending_mutex);
  m_invoke_pending = false;
  m_invoke_pending_cv.wait(lock, [this](){ return m_invoke_pending; });
  return m_running;
}

void EventLoopThread::invoke_pending()
{
  std::unique_lock<std::mutex> lock(m_loop_mutex);
  ev_invoke_pending(EV_A);
  // Notify ev_run_thread.
  m_invoke_handled = true;
  m_invoke_handled_cv.notify_one();
}

void EventLoopThread::handle_invoke_pending()
{
  // The lock is already locked when we get here.
  std::unique_lock<std::mutex> lock(m_loop_mutex, std::adopt_lock);
  while (ev_pending_count(EV_A))
  {
    // Wake up external thread.
    std::unique_lock<std::mutex> lock(m_invoke_pending_mutex);
    m_invoke_pending = true;
    m_invoke_pending_cv.notify_one();

    // Wait until invoke_pending() was called.
    m_invoke_handled = false;
    m_invoke_handled_cv.wait(lock, [this](){ return m_invoke_handled; });
  }
  // Leave the mutex locked.
  lock.release();
}

EventLoopThread::EventLoopThread() : loop(EV_DEFAULT)
{
  // Associate `this` with the loop.
  ev_set_userdata(EV_A_ this);
  ev_set_loop_release_cb(EV_A_ EventLoopThread::release, EventLoopThread::acquire);
  ev_set_invoke_pending_cb(EV_A_ EventLoopThread::handle_invoke_pending);
  // Add an async watcher, this is used in add() to wake up the thread.
  ev_async_init(&m_async_w, EventLoopThread::async_cb);
  ev_async_start(EV_A_ &m_async_w);

  // Create the thread running ev_run.
  m_event_thread = std::thread([this](){ EventLoopThread::main(EV_A); });

  // Wait till we're actually running.
  while (!m_running)
    ;
}

EventLoopThread::~EventLoopThread()
{
  DoutEntering(dc::notice, "EventLoopThread::~EventLoopThread()");
  Dout(dc::notice, "Joining m_event_thread.");
  m_event_thread.join();
}

void EventLoopThread::add(ev_timer& timeout_watcher)
{
  std::lock_guard<std::mutex> lock(m_loop_mutex);
  ev_timer_start(EV_A_ &timeout_watcher);
  ev_async_send(EV_A_ &m_async_w);
}

void EventLoopThread::add(ev_io& io_watcher)
{
  std::lock_guard<std::mutex> lock(m_loop_mutex);
  ev_io_start(EV_A_ &io_watcher);
  ev_async_send(EV_A_ &m_async_w);
}

//----------------------------------------------------------------------------
// Start test program.

// Callback for a time-out.
static void timeout_cb(EV_P_ ev_timer* w, int revents)
{
  Dout(dc::notice, "Calling timeout_cb()!");
  // This causes all nested ev_run's to stop iterating.
  ev_break(EV_A_ EVBREAK_ONE);
}

// Callback for stdin.
static void stdin_cb(EV_P_ ev_io* w, int revents)
{
  Dout(dc::notice, "Calling stdin_cb()");
  // For one-shot events, one must manually stop the watcher with its corresponding stop function.
  ev_io_stop(EV_A_ w);
  // This causes all nested ev_run's to stop iterating.
  ev_break(EV_A_ EVBREAK_ALL);
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  // Create the IO event loop thread.
  EventLoopThread evio_loop;

  // Add a timer watcher.
  ev_timer timeout_watcher;
  ev_timer_init(&timeout_watcher, timeout_cb, 5.5, 0.);
  evio_loop.add(timeout_watcher);

  // Add stdin watcher.
  ev_io stdin_watcher;
  ev_io_init(&stdin_watcher, stdin_cb, /*STDIN_FILENO*/ 0, EV_READ);
  evio_loop.add(stdin_watcher);

  Dout(dc::notice, "Entering main loop.");
  while (evio_loop.wait_for_events())
    evio_loop.invoke_pending();
  Dout(dc::notice, "Leaving main().");
}
