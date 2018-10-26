#include "sys.h"
#include "debug.h"
#include "evio/SocketAddress.h"

int main(int, char* argv[])
{
  Debug(NAMESPACE_DEBUG::init());

  evio::SocketAddress sa(argv[1]);

  evio::SocketAddress::arpa_buf_t arpa_buf;
  sa.ptr_qname(arpa_buf);

  Dout(dc::notice, "sa = " << sa << "; ptr qname = \"" << arpa_buf.data() << "\".");
}
