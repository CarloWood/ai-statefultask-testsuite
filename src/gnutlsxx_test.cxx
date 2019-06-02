char const* SERVER = "217.69.76.59";    // www.gnutls.org, that I'm just using as a test server ;).
int const   PORT = 443;

// This file was created with:
// echo -n | openssl s_client -connect www.gnutls.org:443 | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > ca.pem
#define CAFILE "ca.pem"
// Load all ubuntu certificates.
#define CADIR "/etc/ssl/certs"

#define MSG "GET / HTTP/1.0\r\n\r\n"
#define MAX_BUF 1024

#include "sys.h"
#include "debug.h"
#include "utils/AIAlert.h"
#include <gnutls/gnutls.h>
#include <gnutls/gnutlsxx.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#ifdef CWDEBUG
#include "utils/debug_ostream_operators.h"
#include <libcwd/buf2str.h>
#endif

// Use Berkeley sockets.
#include <sys/socket.h>         // socket, connect, shutdown, AF_INET, SOCK_STREAM, SHUT_RDWR.
#include <unistd.h>             // close.
#include <arpa/inet.h>          // htons, inet_pton, struct sockaddr_in.

namespace gnutls {

struct error_codes
{
  int mCode;

  error_codes() DEBUG_ONLY(: mCode(GNUTLS_E_APPLICATION_ERROR_MIN)) { }
  error_codes(int code) : mCode(code) { }
  operator int() const { return mCode; }
};

std::error_code make_error_code(error_codes);

} // namespace gnutls

// Register gnutls::error_codes as valid error code.
namespace std {

template<> struct is_error_code_enum<gnutls::error_codes> : true_type { };

} // namespace std

// Connects to the peer and returns a socket descriptor.
int tcp_connect()
{
  // Connect to server.
  int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(PORT);
  inet_pton(AF_INET, SERVER, &sa.sin_addr);

  int err = connect(sd, (struct sockaddr*) &sa, sizeof(sa));
  if (err < 0 && errno != EINPROGRESS)
  {
    DoutFatal(dc::fatal|error_cf, "Connect error");
  }

  return sd;
}

class Credentials : public gnutls::certificate_credentials
{
 public:
  int set_x509_system_trust()
  {
    DoutEntering(dc::notice, "Credentials::set_x509_system_trust()");
    int ret = gnutls_certificate_set_x509_system_trust(cred);
    if (ret < 0)
      THROW_ALERTC(static_cast<gnutls::error_codes>(-ret), "gnutls_certificate_set_x509_system_trust");
    Dout(dc::notice, "Number of credentials processed: " << ret);
    return ret;
  }

  void set_x509_trust_dir(char const* ca_dir, gnutls_x509_crt_fmt_t type)
  {
    gnutls::error_codes ret = gnutls_certificate_set_x509_trust_dir(cred, ca_dir, type);
    if (ret < 0)
      THROW_ALERTC(-ret, "gnutls_certificate_set_x509_trust_dir with ca_dir = \"[CA_DIR]\"", AIArgs("[CA_DIR]", ca_dir));
  }
};

class ClientSession : public gnutls::client_session
{
 public:
  void set_default_priority()
  {
    set_priority(nullptr, nullptr);
  }

  gnutls::error_codes handshake(bool rehandshake = false)
  {
    DoutEntering(dc::notice, "ClientSession::handshake(" << rehandshake << ")");
    gnutls::error_codes ret;
    do
    {
      ret = gnutls_handshake(s);
    }
    while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_WARNING_ALERT_RECEIVED || (rehandshake && ret == GNUTLS_E_GOT_APPLICATION_DATA));
    return ret; // Either 0 (GNUTLS_E_SUCCESS) or GNUTLS_E_AGAIN is returned. Other negative values are fatal errors.
  }

  ssize_t send(void const* data, size_t sizeofdata)
  {
    DoutEntering(dc::notice, "ClientSession::send()");
    ssize_t len;
    do
    {
      len = gnutls_record_send(s, data, sizeofdata);
    }
    while (0);
    return len;
  }

  ssize_t recv(void* data, size_t sizeofdata)
  {
    DoutEntering(dc::notice, "ClientSession::recv()");
    ssize_t len;
    do
    {
      len = gnutls_record_recv(s, data, sizeofdata);
    }
    while (len == GNUTLS_E_INTERRUPTED);
    return len; // Either the received length, GNUTLS_E_REHANDSHAKE or GNUTLS_E_AGAIN is returned. Other negative values are fatal errors.
  }

  gnutls::error_codes bye(gnutls_close_request_t how)
  {
    DoutEntering(dc::notice, "ClientSession::bye()");
    gnutls::error_codes ret;
    do
    {
      ret = gnutls_bye(s, how);
    }
    while (ret == GNUTLS_E_INTERRUPTED);
    return ret; // Either 0 (GNUTLS_E_SUCCESS) or GNUTLS_E_AGAIN is returned. Other negative values are errors.
  }
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  // Initialize the gnutls library.
  Dout(dc::notice, "Calling gnutls_global_init()");
  gnutls_global_init();

  // Prepare some socket.
  int sd = tcp_connect();

  try
  {
    // Allow connections to servers that have OpenPGP keys as well.
    Dout(dc::notice, "Constructing ClientSession");
    ClientSession session;

    // X509 stuff.
    Dout(dc::notice, "Constructing Credentials");
    Credentials credentials;

    // Sets the trusted cas file.
//    credentials.set_x509_trust_file(CAFILE, GNUTLS_X509_FMT_PEM);

    // Sets the trusted cas dir.
//    credentials.set_x509_trust_dir(CADIR, GNUTLS_X509_FMT_PEM);
    credentials.set_x509_system_trust();

    // Put the x509 credentials to the current session.
    Dout(dc::notice, "Calling session.set_credentials");
    session.set_credentials(credentials);

    // Use default priorities.
    Dout(dc::notice, "Calling session.set_default_priority");
    session.set_default_priority();

    // Connect to the peer.
    Dout(dc::notice, "Calling session.set_transport_ptr");
    session.set_transport_ptr((gnutls_transport_ptr_t) (ptrdiff_t)sd);

    // Perform the TLS handshake.
    gnutls::error_codes ret;
    do
    {
      ret = session.handshake();
      if (ret == GNUTLS_E_AGAIN)
        Dout(dc::warning, "Received EGAIN.");
    }
    while (ret == GNUTLS_E_AGAIN);
    if (ret < 0)
      THROW_ALERTC(-ret, "ClientSession::handshake");

    Dout(dc::notice, "- Handshake was completed");

    // Send the message to the server.
    ssize_t len;
    do
    {
      len = session.send(MSG, strlen(MSG));
      if (len == GNUTLS_E_AGAIN)
        Dout(dc::warning, "Received EGAIN.");
    }
    while (len == GNUTLS_E_AGAIN);
    if (len < 0)
      THROW_ALERTC(static_cast<gnutls::error_codes>(-len), "ClientSession::send");

    // Receive a reply.
    char buffer[MAX_BUF + 1];
    for (int i = 0; i < 2; ++ i)
    {
      do
      {
        len = session.recv(buffer, MAX_BUF);
        if (len == GNUTLS_E_AGAIN)
          Dout(dc::warning, "Received EGAIN.");
        else if (len == GNUTLS_E_REHANDSHAKE)
        {
          len = GNUTLS_E_AGAIN; // Just ignore for now.
        }
        if (len == 0)
          THROW_ALERT("Peer has closed the TLS connection");
      }
      while (len == GNUTLS_E_AGAIN);
      if (len < 0)
        THROW_ALERTC(static_cast<gnutls::error_codes>(-len), "gnutls::client_session::recv");

      Dout(dc::notice, "- Received " << len << " bytes: \"" << libcwd::buf2str(buffer, len) << '"');
    }

    // Terminate session.
    do
    {
      ret = session.bye(GNUTLS_SHUT_RDWR);
      if (ret == GNUTLS_E_AGAIN)
        Dout(dc::warning, "Received EGAIN.");
    }
    while (ret == GNUTLS_E_AGAIN);
    if (ret < 0)
      THROW_ALERTC(-ret, "gnutls::client_session::recv");
  }
  catch (AIAlert::Error const& error)
  {
    Dout(dc::warning, error);
  }
  catch (gnutls::exception const& ex)
  {
    Dout(dc::warning, "gnutls::exception caught: " << gnutls_strerror(const_cast<gnutls::exception&>(ex).get_code()));
  }
  catch (std::exception& ex)
  {
    Dout(dc::warning, "gnutls std::exception caught: " << ex.what());
  }

  // Close the socket layer.
  if (sd != -1)
  {
    shutdown(sd, SHUT_RDWR); // No more receptions.
    close(sd);
  }

  // De-initialize libgnutls.
  gnutls_global_deinit();

  Dout(dc::notice, "Leaving main()...");
}

//----------------------------------------------------------------------------
// gnutls error codes.

namespace gnutls {
namespace {

struct GNUTLSErrorCategory : std::error_category
{
  char const* name() const noexcept override;
  std::string message(int ev) const override;
};

char const* GNUTLSErrorCategory::name() const noexcept
{
  return "gnutls";
}

std::string GNUTLSErrorCategory::message(int ev) const
{
  return gnutls_strerror(ev);
}

GNUTLSErrorCategory const theGNUTLSErrorCategory { };

} // namespace

std::error_code make_error_code(error_codes code)
{
  return std::error_code(static_cast<int>(code), theGNUTLSErrorCategory);
}

} // namespace gnutls
