#include "sys.h"
#include "debug.h"
#include "socket-task/ConnectToEndPoint.h"
#include "threadpool/AIThreadPool.h"
#include "statefultask/AIEngine.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "threadsafe/Condition.h"
#include <libcwd/buf2str.h>

class InputPrinter : public evio::InputDecoder
{
 protected:
   evio::RefCountReleaser decode(evio::MsgBlock&& msg, evio::GetThread type) override;
};

evio::RefCountReleaser InputPrinter::decode(evio::MsgBlock&& msg, evio::GetThread)
{
  evio::RefCountReleaser releaser;
  // Just print what was received.
  DoutEntering(dc::notice, "InputPrinter::decode(\"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');
  // Stop when ...
  if (msg.get_size() >= 17 && strncmp(msg.get_start() + msg.get_size() - 17, "#5</body></html>\n", 17) == 0)
    releaser = stop_input_device();
  return releaser;
}

class MySocket : public evio::Socket
{
 private:
  InputPrinter m_input_printer;
  evio::OutputStream m_output_stream;

 public:
  MySocket()
  {
    input(m_input_printer);
    output(m_output_stream);
  }

  evio::OutputStream& output_stream() { return m_output_stream; }
};

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  try
  {
    resolver::Resolver::instance().init(handler, false);
    evio::EventLoop event_loop(handler);

    // Allow the main thread to wait until the test finished.
    aithreadsafe::Condition test_finished;

    boost::intrusive_ptr<task::ConnectToEndPoint> task = new task::ConnectToEndPoint(DEBUG_ONLY(true));
    auto socket = evio::create<MySocket>();
    task->set_socket(socket);
    task->set_end_point(AIEndPoint("www.google.com", 80));
    task->run([task, &test_finished](bool success){
          if (!success)
            Dout(dc::warning, "task::ConnectToEndPoint was aborted");
          else
          {
            Dout(dc::notice, "Task with endpoint " << task->get_end_point() << " finished.");
          }
          test_finished.signal();
        });
    // Must do a flush or else the buffer won't be written to to the socket at all; this flush
    // does not block though, it only starts watching the fd for readability and then writes
    // the buffer to the fd when possible.
    // If the socket was closed in the meantime because it permanently failed to connect
    // or connected but then the connection was terminated for whatever reason; then the
    // flush will print a debug output (WARNING: The device is not writable!) and the contents
    // of the buffer are discarded.
    socket->output_stream() << "GET / HTTP/1.0\r\n\r\n"; // << std::flush;

    // Wait until the test is finished.
    std::lock_guard<AIMutex> lock(test_finished);
    test_finished.wait();
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  // Terminate application.
  resolver::Resolver::instance().close();

  Dout(dc::notice, "Leaving main()...");
}
