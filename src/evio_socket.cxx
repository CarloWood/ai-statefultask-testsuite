// Run this test while './http_server' from the project https://github.com/CarloWood/curl_http_pipeline_tester.git is already running.

#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/Device.h"
#include "libcwd/buf2str.h"

#include <cstdio>       // Needed for sprintf.
#include <cstring>	// Needed for memset.
#include <cerrno>

#include <netdb.h>      // Needed for gethostbyname() */
#include <sys/time.h>
#include <sys/types.h>  // Needed for socket, send etc.
#include <sys/socket.h> // Needed for socket, send etc.
#include <netinet/in.h> // Needed for htons.
#include <arpa/inet.h>  // Needed for inet_ntoa.
#include <unistd.h>     // Needed for close.
#include <fcntl.h>      // Needed for fcntl.

int connect_to_server(char const* remote_host, int remote_port);

class Socket : public evio::InputDevice, public evio::OutputDevice
{
 private:
  int m_request;

 public:
  Socket() : evio::InputDevice(new evio::InputBuffer(evio::InputDevice::default_blocksize_c)),
             evio::OutputDevice(new evio::OutputBuffer(evio::OutputDevice::default_blocksize_c)),
             m_request(0) { }

 protected:
  void read_from_fd(int fd) override;
  void write_to_fd(int fd) override;
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

int print_hostent(struct hostent* h)
{
  Dout(dc::notice, "The official name of the host: \"" << h->h_name << "\".");
  if (h->h_aliases[0])
    Dout(dc::notice, "Aliases:");
  else
    Dout(dc::notice, "No aliases.");
  for (int c = 0; h->h_aliases[c]; ++c)
    Dout(dc::notice, "\"" << h->h_aliases[c] << "\".");
  if (h->h_addrtype != AF_INET)
  {
    std::cerr << "Returned address type is not AF_INET !?\n";
    return -1;
  }
  Dout(dc::notice, "Address length in bytes: " << h->h_length << '.');
  if (h->h_addr_list[0])
    Dout(dc::notice, "Network addresses:");
  else
    Dout(dc::notice, "No network addresses.");
  for (int c = 0; h->h_addr_list[c]; ++c)
    Dout(dc::notice, "\"" << inet_ntoa(*(struct in_addr *)h->h_addr_list[c]) << "\".");
  return 0;
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
  if (print_hostent(hp))
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
  }

  Dout(dc::notice, "\"Connected\".");
  return fd_remote;
}

void Socket::read_from_fd(int fd)
{
  DoutEntering(dc::notice, "Socket::read_from_fd(" << fd << ")");
  char buf[256];
  ssize_t len;
  do
  {
    len = read(fd, buf, 256);
    Dout(dc::notice, "Read: \"" << libcwd::buf2str(buf, len) << "\".");
  }
  while (len == 256);
  if (strncmp(buf + len - 17, "#5</body></html>\n", 17) == 0)
  {
    stop_input_device();
    ev_break(EV_A_ EVBREAK_ALL);
  }
}

void Socket::write_to_fd(int fd)
{
  DoutEntering(dc::notice, "Socket::write_to_fd(" << fd << ")");
  if (m_request < 6)
  {
    std::stringstream ss;
    ss << "GET / HTTP/1.1\r\nHost: localhost:9001\r\nAccept: */*\r\nX-Request: " << m_request << "\r\nX-Sleep: " << (200 * m_request) << "\r\n\r\n";
    ++m_request;
    write(fd, ss.str().data(), ss.str().length());
    Dout(dc::notice, "Wrote \"" << libcwd::buf2str(ss.str().data(), ss.str().length()) << "\".");
  }
  else
    stop_output_device();
}
