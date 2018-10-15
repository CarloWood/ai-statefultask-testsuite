// Run this test while './http_server' from the project https://github.com/CarloWood/curl_http_pipeline_tester.git is already running.

#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/Device.h"
#include "evio/inet_support.h"

#include <sstream>
#include <cstring>	// Needed for memset.
#include <cerrno>

#include <netdb.h>      // Needed for gethostbyname() */
#include <sys/time.h>
#include <sys/types.h>  // Needed for socket, send etc.
#include <sys/socket.h> // Needed for socket, send etc.
#include <netinet/in.h> // Needed for htons.

#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

int connect_to_server(char const* remote_host, int remote_port);

class Socket : public evio::InputDevice, public evio::OutputDevice
{
 private:
  int m_request;

 public:
  Socket() : evio::InputDevice(new evio::InputBuffer), evio::OutputDevice(new evio::OutputBuffer), m_request(0) { }

 protected:
  void read_from_fd(int fd) override;   // Read thread.
  void write_to_fd(int fd) override;    // Write thread.
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
    boost::intrusive_ptr<Socket> fdp0 = new Socket;

    int fd = connect_to_server("localhost", 9001);

    fdp0->init(fd);
    fdp0->start();
  }

  // Wait until all watchers have finished.
  EventLoopThread::instance().terminate();
}

int connect_to_server(char const* remote_host, int remote_port)
{
  // Get host by name.
  struct hostent* hp;
  if (!(hp = gethostbyname(remote_host)))
  {
    herror("gethostbyname");
    return -1;
  }
  // Dump the info we got.
  if (evio::print_hostent_on(hp, std::cout))
    exit(-1);

  struct sockaddr_in remote_addr;
  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(remote_port);
  memcpy((char*)&remote_addr.sin_addr, (char*)hp->h_addr_list[0], sizeof(struct in_addr));

  Dout(dc::notice, "Connecting to port " << remote_port);

  // Create socket to remote host.
  int fd_remote;
  if ((fd_remote = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket");
    exit(-1);
  }

  int opt = 512;
  if (setsockopt(fd_remote, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt");
    exit(-1);
  }
  opt = 512;
  if (setsockopt(fd_remote, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt");
    exit(-1);
  }

  evio::set_nonblocking(fd_remote);

  // Connect the socket.
  if (connect(fd_remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0)
  {
    int err = errno;
    perror("connect");
    if (err != EINPROGRESS)
      exit(-1);
    Dout(dc::notice, "\"Connect in progress\".");
  }
  else
    Dout(dc::notice, "\"Connected\".");
  return fd_remote;
}

// Read thread.
void Socket::read_from_fd(int fd)
{
  DoutEntering(dc::notice, "Socket::read_from_fd(" << fd << ")");
  RefCountReleaser releaser;
  char buf[256];
  ssize_t len;
  do
  {
    Dout(dc::system|continued_cf, "read(" << fd << ", char[256], 256) = ");
    len = read(fd, buf, 256);
    Dout(dc::finish|cond_error_cf(len == -1), len);
    if (len == -1)
    {
      releaser = evio::InputDevice::close();
      return;
    }
    Dout(dc::notice, "Read: \"" << libcwd::buf2str(buf, len) << "\".");
  }
  while (len == 256);
  if (strncmp(buf + len - 17, "#5</body></html>\n", 17) == 0)
  {
    releaser += std::move(stop_input_device());
    ev_break(EV_A_ EVBREAK_ALL);
  }
}

// Write thread.
void Socket::write_to_fd(int fd)
{
  DoutEntering(dc::notice, "Socket::write_to_fd(" << fd << ")");
  if (m_request < 6)
  {
    std::stringstream ss;
    ss << "GET / HTTP/1.1\r\nHost: localhost:9001\r\nAccept: */*\r\nX-Request: " << m_request << "\r\nX-Sleep: " << (200 * m_request) << "\r\n\r\n";
    ++m_request;
    [[maybe_unused]] int unused = write(fd, ss.str().data(), ss.str().length());
    Dout(dc::notice, "Wrote \"" << libcwd::buf2str(ss.str().data(), ss.str().length()) << "\".");
  }
  else
    stop_output_device();
}
