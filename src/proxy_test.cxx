#include "sys.h"
#include "socket-task/ConnectToEndPoint.h"
#include "statefultask/AIEngine.h"
#include "statefultask/DefaultMemoryPagePool.h"
#include "evio/EventLoop.h"
#include "evio/ListenSocket.h"
#include "utils/threading/Gate.h"
#include "utils/AIAlert.h"
#include "utils/debug_ostream_operators.h"
#include "debug.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

namespace utils { using namespace threading; }

std::mutex cout_mutex;

class DBusProtocol : public evio::Protocol
{
};

class DBusSocket : public evio::Socket
{
 protected:
  void data_received(int& allow_deletion_count, char const* new_data, size_t rlen) override
  {
    {
      std::lock_guard<std::mutex> lk(cout_mutex);
      std::cout << "dbus--> " << utils::print_using(std::string_view{new_data, rlen}, utils::c_escape) << std::endl;
    }
    evio::Socket::data_received(allow_deletion_count, new_data, rlen);
  }
};

class ProxyAcceptedSocket : public evio::Socket
{
 private:
  boost::intrusive_ptr<DBusSocket> m_dbus_socket;       // Temporary pointer, until moved away in new_connection.
  bool m_connected;

 public:
  using input_protocol_type = DBusProtocol;
  using output_protocol_type = DBusProtocol;

  void init(int fd, evio::SocketAddress const& remote_address)
  {
    m_dbus_socket = evio::create<DBusSocket>();
    set_source(m_dbus_socket, DBusProtocol().minimum_block_size());
    m_dbus_socket->set_source(boost::intrusive_ptr<ProxyAcceptedSocket>{this}, DBusProtocol().minimum_block_size());
    m_connected = false;
    evio::Socket::init(fd, remote_address);
  }

  void connected(bool success)
  {
    if (success)
      m_connected  = true;
    else
      close();
  }

  boost::intrusive_ptr<DBusSocket>& dbus_socket() { return m_dbus_socket; }

 protected:
  void data_received(int& allow_deletion_count, char const* new_data, size_t rlen) override
  {
    {
      std::lock_guard<std::mutex> lk(cout_mutex);
      std::cout << "dbus<-- " << utils::print_using(std::string_view{new_data, rlen}, utils::c_escape) << std::endl;
    }
    // Wait until the outgoing socket is connected (and thus initialized), otherwise
    // the call to data_received below might cause a call to start_output_device
    // before the outgoing socket (DBusSocket) is even initialized, leading to an assert.
    while (!m_connected)
    {
      Dout(dc::notice, "Waiting till DBusSocket is connected...");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    evio::Socket::data_received(allow_deletion_count, new_data, rlen);
  }
};

class ProxyListenSocket : public evio::ListenSocket<ProxyAcceptedSocket>
{
 private:
  evio::SocketAddress m_dbus_address;
  utils::Gate& m_test_finished;

 public:
  ProxyListenSocket(evio::SocketAddress dbus_address, utils::Gate& test_finished) : m_dbus_address(std::move(dbus_address)), m_test_finished(test_finished) { }

  // Called when a new connection is accepted.
  void new_connection(accepted_socket_type& accepted_socket) override
  {
    Dout(dc::notice, "New connection to listen socket was accepted.");

    boost::intrusive_ptr<task::ConnectToEndPoint> task = new task::ConnectToEndPoint(CWDEBUG_ONLY(true));
    task->set_socket(std::move(accepted_socket.dbus_socket()));
    task->set_end_point(m_dbus_address);
    task->on_connected([&accepted_socket](bool success){ accepted_socket.connected(success); });
    boost::intrusive_ptr<ProxyListenSocket> self(this);
    task->run([task, self](bool success){
          if (!success)
            Dout(dc::warning, "task::ConnectToEndPoint was aborted");
          else
            Dout(dc::notice, "task::ConnectToEndPoint with endpoint " << task->get_end_point() << " finished.");
          auto ref_count_releaser = self->close();
          self->m_test_finished.open();
        });
  }
};

int constexpr queue_capacity = 32;

int main()
{
//  Debug(NAMESPACE_DEBUG::init());

  AIMemoryPagePool mpp;
  AIThreadPool thread_pool;
  Debug(thread_pool.set_color_functions([](int color){ std::string code{"\e[30m"}; code[3] = '1' + color; return code; }));
  AIQueueHandle handler = thread_pool.new_queue(queue_capacity);

  static evio::SocketAddress const listen_address("/run/user/1000/proxy");
  static evio::SocketAddress const dbus_address("/run/user/1000/bus");

  try
  {
    evio::EventLoop event_loop(handler);
    resolver::Scope resolver_scope(handler, false);

    // Allow the main thread to wait until the test finished.
    utils::Gate test_finished;

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
