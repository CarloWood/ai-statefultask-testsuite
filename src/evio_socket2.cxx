// Run this test while './http_server' from the project https://github.com/CarloWood/curl_http_pipeline_tester.git is already running.

#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/Socket.h"
#include "evio/inet_support.h"
#include "libcwd/buf2str.h"

#include <sstream>
#include <cstring>	// Needed for memset.
#include <cerrno>

#include <netdb.h>      // Needed for gethostbyname() */
#include <sys/time.h>
#include <sys/types.h>  // Needed for socket, send etc.
#include <sys/socket.h> // Needed for socket, send etc.
#include <netinet/in.h> // Needed for htons.

class ReadSocket : public evio::ReadInputDevice
{
 public:
  ReadSocket(evio::InputBuffer* ibuffer) : evio::ReadInputDevice(ibuffer) { }

 protected:
  size_t end_of_msg_finder(char const*, size_t) override;
  RefCountReleaser decode(evio::MsgBlock msg) override;
};

class WriteSocket : public evio::WriteOutputDeviceStream
{
 private:
  int m_request;

 public:
  WriteSocket(evio::OutputBuffer* obuffer) : evio::WriteOutputDeviceStream(obuffer), m_request(0) { }
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
    auto sockstream = evio::create<evio::Socket<ReadSocket, WriteSocket>>();
    sockstream->connect("localhost", (unsigned short)9001);

    for (int request = 0; request < 6; ++request)
      *sockstream << "GET / HTTP/1.1\r\n"
                     "Host: localhost:9001\r\n"
                     "Accept: */*\r\n"
                     "X-Request: " << request << "\r\n"
                     "X-Sleep: " << (200 * request) << "\r\n"
                     "\r\n" << std::flush;
  }

  // Wait until all watchers have finished.
  EventLoopThread::instance().terminate();
}

size_t ReadSocket::end_of_msg_finder(char const* start, size_t count)
{
  char const* newline = static_cast<char const*>(std::memchr(start, '\n', count));
  return newline ? newline - start + 1 : 0;
}

evio::IOBase::RefCountReleaser ReadSocket::decode(evio::MsgBlock msg)
{
  evio::IOBase::RefCountReleaser releaser;
  DoutEntering(dc::notice, "ReadSocket::decode(\"" << buf2str(msg.get_start(), msg.get_size()) << "\")");
  if (msg.get_size() >= 17 && strncmp(msg.get_start() + msg.get_size() - 17, "#5</body></html>\n", 17) == 0)
    releaser = stop_input_device();
  return releaser;
}
