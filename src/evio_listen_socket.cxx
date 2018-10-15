#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/ListenSocket.h"
#include "evio/Socket.h"
#include "statefultask/Timer.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

using evio::text;
using evio::ReadInputDevice;
using evio::InputBuffer;
using evio::OutputBuffer;
using evio::MsgBlock;
using evio::OStreamDevice;
using evio::Socket;
using evio::ListenSocket;
template<statefultask::Timer::time_point::rep count, typename Unit> using Interval = statefultask::Interval<count, Unit>;

class Decoder : public ReadInputDevice<text>
{
 private:
  size_t m_received;

 public:
  Decoder(InputBuffer* ibuffer) : ReadInputDevice<text>(ibuffer), m_received(0) { }

 protected:
  RefCountReleaser decode(MsgBlock msg) override;
};

class MyListenSocket : public ListenSocket<Socket<Decoder, OStreamDevice>>
{
 public:
  MyListenSocket(unsigned short port) : ListenSocket<Socket<Decoder, OStreamDevice>>(new InputBuffer(8192), port) { }

 protected:
  void new_connection(Socket<Decoder, OStreamDevice>& connection)
  {
    // Write 10 kbyte of data.
    for (int n = 0; n < 100; ++n)
      connection << "START012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789THEEND" << std::endl;
  }
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle high_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle medium_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle low_priority_handler = thread_pool.new_queue(16);

  // Initialize the IO event loop thread.
  EventLoopThread::instance().init(low_priority_handler);

  // Start a listen socket on port 9001 that is closed after 10 seconds.
  statefultask::Timer timer;
  {
    auto listen_sock = evio::create<MyListenSocket>(9001);
    timer.start(Interval<10, std::chrono::seconds>(),
        [&timer, listen_sock]()
        {
          timer.release_callback();
          listen_sock->close();
          EventLoopThread::instance().bump_terminate();
        });
  }

  // Dumb way to way until the listen socket is up.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  {
    // Connect 100 sockets to it.
    std::array<boost::intrusive_ptr<Socket<Decoder, OStreamDevice>>, 100> sockets;
    for (size_t s = 0; s < sockets.size(); ++s)
      sockets[s] = evio::create<Socket<Decoder, OStreamDevice>>();
    for (size_t s = 0; s < sockets.size(); ++s)
      sockets[s]->connect("127.0.0.1", (unsigned short)9001);
  }

  // Wait until all watchers have finished.
  EventLoopThread::instance().terminate();
}

evio::IOBase::RefCountReleaser Decoder::decode(MsgBlock msg)
{
  RefCountReleaser releaser;
  // Just print what was received.
  DoutEntering(dc::notice, "Decoder::decode(\"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');
  m_received += msg.get_size();
  // Stop when the last message was received.
  if (m_received == 10200)
    releaser = stop_input_device();
  return releaser;
}
