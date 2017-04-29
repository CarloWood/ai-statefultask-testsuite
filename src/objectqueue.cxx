#include "sys.h"
#include "debug.h"
#include "tracked.h"
#include "statefultask/AIObjectQueue.h"
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdlib>

namespace { constexpr char const* const name_F = "std::function<void()>"; }
struct F : tracked::Tracked<&name_F> {
  std::function<void()> m_f;
  F() {}
  template<typename BIND>
  F(BIND&& bind) : m_f(std::forward<BIND>(bind)) { }
  F(F& orig) : tracked::Tracked<&name_F>{orig}, m_f{orig.m_f} { }
  F(F const& orig) : tracked::Tracked<&name_F>{orig}, m_f{orig.m_f} { }
  F(F&& orig) : tracked::Tracked<&name_F>{std::move(orig)}, m_f{std::move(orig.m_f)} { }
  F(F const&& orig) : tracked::Tracked<&name_F>{std::move(orig)}, m_f{std::move(orig.m_f)} { }
  void operator=(F const& orig) { tracked::Tracked<&name_F>::operator=(orig); m_f = orig.m_f; }
  void operator=(F&& orig) { tracked::Tracked<&name_F>::operator=(std::move(orig)); m_f = std::move(orig.m_f); }
  operator bool() const { return static_cast<bool>(m_f); }
  void operator()() const { m_f(); }
};

namespace { constexpr char const* const name_B = "Big"; }
struct B : tracked::Tracked<&name_B> {
  using tracked::Tracked<&name_B>::Tracked;
};

struct Big {
  char m_data[64] = "Big";
  B m_tracker;
};

void f(Big const& b)
{
  Dout(dc::notice, "Calling f(\"" << b.m_data << "\")!");
}

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif

  Debug(NAMESPACE_DEBUG::init());
  //Debug(tracked::mute());

  {
    // Construct an empty buffer.
    Dout(dc::notice|continued_cf, "Constructing AIObjectQueue<" << name_F << "> object_queue... ");
    AIObjectQueue<F> object_queue;
    Dout(dc::finish, "done.");

    try {
      // Allocate 10 std::function<void()> objects in it.
      object_queue.reallocate(10);
    } catch (std::bad_alloc const& e) {
      std::cout << "Allocation failed: " << e.what() << '\n';
    }

    auto pa = object_queue.producer_access();
    auto ca = object_queue.consumer_access();

    pa.clear();   // Access test (buffer is already empty of course).
    ca.clear();

    {
      Dout(dc::notice|continued_cf, "Constructing Big b... ");
      Big b;
      Dout(dc::finish, "done.");

      {
        Dout(dc::notice|continued_cf, "Constructing " << name_F << " big_function(std::bind(f, b))... ");
        F function(std::bind(f, b));
        Dout(dc::finish, "done.");

        //Debug(tracked::unmute());
        Dout(dc::notice|continued_cf, "Moving " << name_F << " into AIObjectQueue... ");
        pa.move_in(std::move(function));
        Dout(dc::finish, "Done.");

        {
          Dout(dc::notice|continued_cf, "Moving " << name_F << " out of AIObjectQueue to bf... ");
          F bf(ca.move_out());
          Dout(dc::finish, "Done.");

          if (!bf)
          {
            Dout(dc::notice, "Buffer empty!");
          }
          else
          {
            Dout(dc::notice|continued_cf, "Copying bf into AIObjectQueue... ");
            pa.move_in(F(bf));
            Dout(dc::finish, "Done.");
            Dout(dc::notice|continued_cf, "Moving bf into AIObjectQueue... ");
            pa.move_in(std::move(bf));
            Dout(dc::finish, "Done.");
            Dout(dc::notice|continued_cf, "Moving " << name_F << " from AIObjectQueue to bf2... ");
            F bf2(ca.move_out());
            Dout(dc::finish, "Done.");
            Dout(dc::notice|continued_cf, "Executing bf2()...");
            bf2();
            Dout(dc::finish, "Done.");
            Dout(dc::notice|continued_cf, "Destructing bf2... ");
          }
          Dout(dc::finish, "destructed.");

          Dout(dc::notice|continued_cf, "Destructing bf... ");
        }
        Dout(dc::finish, "destructed.");

        Dout(dc::notice|continued_cf, "Destructing " << name_F << " big_function... ");
      }
      Dout(dc::finish, "destructed.");

      Dout(dc::notice|continued_cf, "Destructing Big b... ");
    }
    Dout(dc::finish, "destructed.");

    //Debug(tracked::mute());
    Dout(dc::notice|continued_cf, "Destructing AIObjectQueue<" << name_F << "> object_queue... ");
  }
  Dout(dc::finish, "destructed.");
}
