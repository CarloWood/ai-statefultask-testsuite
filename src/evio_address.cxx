#define BOOST_ASIO_HEADER_ONLY
#include "sys.h"
#include "debug.h"
#include "evio/SocketAddress.h"
#include <boost/asio/ip/address.hpp>
#include <iostream>

void f(boost::asio::ip::address addr)
{
  std::cout << "f(" << addr << ")\n";
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  evio::SocketAddress addr1("[2606:2800:220:1:248:1893:25c8:1946]:0");
  ASSERT(addr1.to_string() == "[2606:2800:220:1:248:1893:25c8:1946]:0");

  evio::SocketAddress addr2("[2001:41c0::645:a65e:60ff:feda:589d]:123");
  ASSERT(addr2.to_string() == "[2001:41c0:0:645:a65e:60ff:feda:589d]:123");

  evio::SocketAddress addr3("[2001:0db8::1:0:0:1]:12345");
  ASSERT(addr3.to_string() == "[2001:db8::1:0:0:1]:12345");

  evio::SocketAddress addr4("[2001:41c0::1]:65535");
  ASSERT(addr4.to_string() == "[2001:41c0::1]:65535");

  evio::SocketAddress addr5("[2606::1]:80");
  ASSERT(addr5.to_string() == "[2606::1]:80");

  evio::SocketAddress addr6("[::1]:1");
  ASSERT(addr6.to_string() == "[::1]:1");

  evio::SocketAddress addr7("[::]:9001");
  ASSERT(addr7.to_string() == "[::]:9001");

  evio::SocketAddress addr8("[::ffff:192:0:10:1]:1234");
  ASSERT(addr8.to_string() == "[::ffff:192:0:10:1]:1234");

  evio::SocketAddress addr9("::ffff:192.0.10.1:1234");
  ASSERT(addr9.to_string() == "[::ffff:192.0.10.1]:1234");

  evio::SocketAddress addr10("10.0.100.255:65535");
  ASSERT(addr10.to_string() == "10.0.100.255:65535");

  evio::SocketAddress addr11("[0010.000.0100.0255]:065535");
  ASSERT(addr11.to_string() == "10.0.100.255:65535");

  evio::SocketAddress addr12(AF_INET6, "192.168.10.1", 42);
  ASSERT(addr12.to_string() == "[::ffff:192.168.10.1]:42");

  evio::SocketAddress addr13(AF_UNIX, "192.168.10.1");
  ASSERT(addr13.to_string() == "192.168.10.1");

  std::cout << "Success!\n";
}
