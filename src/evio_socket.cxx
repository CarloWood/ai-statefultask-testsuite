#include "sys.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#include "evio/FileDescriptor.h"

#include <cstdio>       // Needed for sprintf.
#include <cstring>	// Needed for memset.
#include <cerrno>

#include <netdb.h>      // Needed for gethostbyname() */
#include <sys/time.h>
#include <sys/types.h>  // Needed for socket, send etc.
#include <sys/socket.h> // Needed for socket, send etc.
#include <netinet/in.h> // Needed for htons.
#include <unistd.h>     // Needed for close.
#include <fcntl.h>      // Needed for fcntl.

int connect_to_server(char const* remote_host, int remote_port);

class Socket : public evio::InputDevice, public evio::OutputDevice
{
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle high_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle medium_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle low_priority_handler = thread_pool.new_queue(16);

  // Create the IO event loop thread.
  EventLoopThread evio_loop(low_priority_handler);

  {
    boost::intrusive_ptr<Socket> fdp0 = new Socket;

    int fd = connect_to_server("localhost", 9001);

    fdp0->init(fd);
    fdp0->start(evio_loop);
  }
}

char* inetntoa(struct in_addr* in)
{
  static char buf[16];
  unsigned char* s = (unsigned char*)&in->s_addr;
  int a, b, c, d;

  a = (int)*s++;
  b = (int)*s++;
  c = (int)*s++;
  d = (int)*s++;
  (void)sprintf(buf,"%d.%d.%d.%d", a, b, c, d);

  return buf;
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
    Dout(dc::notice, "\"" << inetntoa((struct in_addr *)h->h_addr_list[c]) << "\".");
  return 0;
}

void set_non_blocking(int fd)
{
#if 1 // NBLOCK_POSIX
  int nonb = O_NONBLOCK;
#elif defined(NBLOCK_BSD)
  int nonb = O_NDELAY;
#endif
#ifdef NBLOCK_SYSV
  // This portion of code might also apply to NeXT.
  int res = 1;
  if (ioctl(fd, FIONBIO, &res) < 0)
    perror("ioctl(fd, FIONBIO)");
#else
  int res;
  if ((res = fcntl(fd, F_GETFL, 0)) == -1)
    perror("fcntl(fd, F_GETFL)");
  else if (fcntl(fd, F_SETFL, res | nonb) == -1)
    perror("fcntl(fd, F_SETL, nonb)");
#endif
  return;
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

  set_non_blocking(fd_remote);

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
