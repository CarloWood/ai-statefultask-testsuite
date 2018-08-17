#include "sys.h"
#include "tracked.h"

char const* name_string = "string";

class string : public tracked::Tracked<&name_string>
{
  using tracked::Tracked<&name_string>::Tracked;

 public:
  string(string& orig) : tracked::Tracked<&name_string>{orig}, m_string(orig.m_string) { }
  string(string const& orig) : tracked::Tracked<&name_string>{orig}, m_string(orig.m_string) { }
  string(string&& orig) : tracked::Tracked<&name_string>{std::move(orig)}, m_string(std::move(orig.m_string)) { }
  string(string const&& orig) : tracked::Tracked<&name_string>{std::move(orig)}, m_string(std::move(orig.m_string)) { }
  void operator=(string const& orig) { tracked::Tracked<&name_string>::operator=(orig); m_string = orig.m_string; }
  void operator=(string&& orig) { tracked::Tracked<&name_string>::operator=(std::move(orig)); m_string = std::move(orig.m_string); }

  string(char const* s) : m_string(s) { }

 private:
  std::string m_string;
};

struct Lookup
{
  string m_hostname;
  string m_servicename;
  Lookup(string&& hostname, string&& servicename) :
      m_hostname(std::move(hostname)),
      m_servicename(std::move(servicename))
      { }
};

struct Resolver
{
 private:
  Lookup* do_request(string&& hostname, string&& servicename);

 public:
  template<typename S1, typename S2>
  typename std::enable_if<
      (std::is_same<S1, string>::value || std::is_convertible<S1, string>::value) &&
      (std::is_same<S2, string>::value || std::is_convertible<S2, string>::value),
      Lookup*>::type
  request(S1&& hostname, S2&& servicename)
  {
    return do_request(std::forward<string>(hostname), std::forward<string>(servicename));
  }
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Debug(dc::tracked.on());

  Resolver resolver;

  Dout(dc::notice|continued_cf, "Calling resolver.request(\"test\")... ");
  Lookup* lookup = resolver.request("hostname", "service");
  Dout(dc::finish, "done");
  delete lookup;

  Dout(dc::notice|continued_cf, "Calling resolver.request(string())... ");
  lookup = resolver.request(string("hostname"), string("service"));
  Dout(dc::finish, "done");
  delete lookup;

  Dout(dc::notice|continued_cf, "Constructing hostname(\"hostname\") and servicename(\"servicename\")... ");
  string hostname("hostname");
  string servicename("servicename");
  Dout(dc::finish, "done");
  Dout(dc::notice|continued_cf, "Calling resolver.request(std::move(hostname), std::move(servicename))... ");
  lookup = resolver.request(std::move(hostname), std::move(servicename));
  Dout(dc::finish, "done");
  delete lookup;

  Dout(dc::notice|continued_cf, "Constructing hostname2(\"hostname\") and servicename2(\"servicename\")... ");
  string hostname2("hostname");
  string servicename2("servicename");
  Dout(dc::finish, "done");
  Dout(dc::notice|continued_cf, "Calling resolver.request(hostname2, servicename2)... ");
  lookup = resolver.request(hostname2, servicename2);
  Dout(dc::finish, "done");
  delete lookup;

  Dout(dc::notice, "Leaving main()...");
}

Lookup* Resolver::do_request(string&& hostname, string&& servicename)
{
  return new Lookup(std::move(hostname), std::move(servicename));
}
