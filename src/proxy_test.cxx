#include "sys.h"
#include "socket-task/ConnectToEndPoint.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "evio/EventLoop.h"
#include "evio/ListenSocket.h"
#include "threadsafe/Gate.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

class DBusProtocol : public evio::Protocol
{
};

class DBusSocket : public evio::Socket
{
};

class ProxyAcceptedSocket : public evio::Socket
{
 private:
  boost::intrusive_ptr<DBusSocket> m_dbus_socket;       // Temporary pointer, until moved away in new_connection.

 public:
  using input_protocol_type = DBusProtocol;
  using output_protocol_type = DBusProtocol;

  void init(int fd, evio::SocketAddress const& remote_address)
  {
    m_dbus_socket = evio::create<DBusSocket>();
    set_source(m_dbus_socket, DBusProtocol().minimum_block_size());
    m_dbus_socket->set_source(boost::intrusive_ptr<ProxyAcceptedSocket>{this}, DBusProtocol().minimum_block_size());
    evio::Socket::init(fd, remote_address);
  }

  boost::intrusive_ptr<DBusSocket>& dbus_socket() { return m_dbus_socket; }
};

class ProxyListenSocket : public evio::ListenSocket<ProxyAcceptedSocket>
{
 private:
  evio::SocketAddress m_dbus_address;
  aithreadsafe::Gate& m_test_finished;

 public:
  ProxyListenSocket(evio::SocketAddress dbus_address, aithreadsafe::Gate& test_finished) : m_dbus_address(std::move(dbus_address)), m_test_finished(test_finished) { }

  // Called when a new connection is accepted.
  void new_connection(accepted_socket_type& accepted_socket) override
  {
    Dout(dc::notice, "New connection to listen socket was accepted.");

    boost::intrusive_ptr<task::ConnectToEndPoint> task = new task::ConnectToEndPoint(CWDEBUG_ONLY(true));
    task->set_socket(std::move(accepted_socket.dbus_socket()));
    task->set_end_point(m_dbus_address);
    task->run([task, this](bool success){
          if (!success)
            Dout(dc::warning, "task::ConnectToEndPoint was aborted");
          else
          {
            Dout(dc::notice, "Task with endpoint " << task->get_end_point() << " finished.");
          }
          m_test_finished.open();
          close();
        });
  }
};

int constexpr queue_capacity = 32;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  static evio::SocketAddress const listen_address("127.0.0.1:9393");
  static evio::SocketAddress const dbus_address("127.0.0.1:25"); // ("/run/user/1000/bus");

  try
  {
    evio::EventLoop event_loop(handler);
    resolver::Scope resolver_scope(handler, false);

    // Allow the main thread to wait until the test finished.
    aithreadsafe::Gate test_finished;

    // Start the listen socket.
    {
      auto listen_sock = evio::create<ProxyListenSocket>(dbus_address, test_finished);
      listen_sock->listen(listen_address);
    }

    // Wait until the test is finished.
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
