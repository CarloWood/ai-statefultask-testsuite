// Run this test while './http_server' from the project https://github.com/CarloWood/curl_http_pipeline_tester.git is already running.

#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/Socket.h"
#include <cstring>	// Needed for memset.
#include <netinet/in.h> // Needed for htons.
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

class Decoder : public ReadInputDevice<text>
{
 public:
  Decoder(InputBuffer* ibuffer) : ReadInputDevice<text>(ibuffer) { }

 protected:
  RefCountReleaser decode(MsgBlock msg) override;
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

  {
    boost::intrusive_ptr<Socket<Decoder, OStreamDevice>> sockstream[4];
    for (int i = 0; i < 4; ++i)
      sockstream[i] = evio::create<Socket<Decoder, OStreamDevice>>();
    for (int i = 0; i < 4; ++i)
      sockstream[i]->connect("localhost", (unsigned short)9001);

    // Write 6 requests to each socket.
    for (int request = 0; request < 6; ++request)
      for (int i = 0; i < 4; ++i)
        *sockstream[i] << "GET / HTTP/1.1\r\n"
                          "Host: localhost:9001\r\n"
                          "Accept: */*\r\n"
                          "X-Request: " << request << "\r\n"
                          "X-Sleep: " << (200 * request) << "\r\n"
                          "\r\n" << std::flush;
  }

  // Wait until all watchers have finished.
  EventLoopThread::instance().terminate();
}

evio::IOBase::RefCountReleaser Decoder::decode(MsgBlock msg)
{
  RefCountReleaser releaser;
  // Just print what was received.
  DoutEntering(dc::notice, "Decoder::decode(\"" << buf2str(msg.get_start(), msg.get_size()) << "\") [" << this << ']');
  // Stop when the last message was received.
  if (msg.get_size() >= 17 && strncmp(msg.get_start() + msg.get_size() - 17, "#5</body></html>\n", 17) == 0)
    releaser = stop_input_device();
  return releaser;
}
