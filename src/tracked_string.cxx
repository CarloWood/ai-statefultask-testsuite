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

struct Foo
{
  string s;
  Foo(string&& s_) : s(std::forward<string>(s_)) { }
};

Foo* f(string&& s)
{
  return new Foo(std::forward<string>(s));
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  Debug(dc::tracked.on());

  Dout(dc::notice|continued_cf, "Calling f(\"test\")... ");
  Foo* foo = f("test");
  Dout(dc::finish, "done");
  delete foo;

  Dout(dc::notice|continued_cf, "Calling f(string())... ");
  foo = f(string());
  Dout(dc::finish, "done");
  delete foo;

  Dout(dc::notice|continued_cf, "Constructing s(\"test\")... ");
  string s("test");
  Dout(dc::finish, "done");
  Dout(dc::notice|continued_cf, "Calling f(std::move(s))... ");
  foo = f(std::move(s));
  Dout(dc::finish, "done");
  delete foo;

#if 0
  Dout(dc::notice|continued_cf, "Constructing s2(\"test\")... ");
  string s2("test");
  Dout(dc::finish, "done");
  Dout(dc::notice|continued_cf, "Calling f(std::move(s))... ");
  foo = f(s2); // This MUST cause one copy.
  Dout(dc::finish, "done");
  delete foo;
#endif

  Dout(dc::notice, "Leaving main()...");
}
