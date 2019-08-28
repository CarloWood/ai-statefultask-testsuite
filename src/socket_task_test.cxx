#include "sys.h"
#include "socket-task/ConnectToEndPoint.h"
#include "statefultask/AIEngine.h"
#include "evio/EventLoop.h"
#include "evio/TLSSocket.h"
#include "threadsafe/Condition.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

class InputPrinter : public evio::InputDecoder
{
 protected:
   void decode(int& allow_deletion_count, evio::MsgBlock&& msg) override;
};

void InputPrinter::decode(int& CWDEBUG_ONLY(allow_deletion_count), evio::MsgBlock&& msg)
{
  // Just print what was received.
  DoutEntering(dc::notice, "InputPrinter::decode({" << allow_deletion_count << "}, \"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');
  // Stop when ...
  if (msg.get_size() >= 17 && strncmp(msg.get_start() + msg.get_size() - 17, "#5</body></html>\n", 17) == 0)
    stop_input_device();
}

class MySocket : public evio::TLSSocket
{
 private:
  InputPrinter m_input_printer;
  evio::OutputStream m_output_stream;

 public:
  MySocket()
  {
    set_sink(m_input_printer);
    set_source(m_output_stream);
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
    evio::EventLoop event_loop(handler);
    resolver::Scope resolver_scope(handler, false);

    // Allow the main thread to wait until the test finished.
    aithreadsafe::Condition test_finished;

    boost::intrusive_ptr<task::ConnectToEndPoint> task = new task::ConnectToEndPoint(DEBUG_ONLY(true));
    auto socket = evio::create<MySocket>();
    task->set_socket(socket);
    task->set_end_point(AIEndPoint("www.google.com", 443));
    task->run([task, &test_finished](bool success){
          if (!success)
            Dout(dc::warning, "task::ConnectToEndPoint was aborted");
          else
          {
            Dout(dc::notice, "Task with endpoint " << task->get_end_point() << " finished.");
          }
          test_finished.signal();
        });
    // Must do a flush or else the buffer won't be written to the socket at all; this flush
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

    // Terminate application.
    event_loop.join();
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }

  Dout(dc::notice, "Leaving main()...");
}
