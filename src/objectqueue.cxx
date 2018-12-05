#include "sys.h"
#include "debug.h"
#ifdef CWDEBUG
#include "tracked.h"
#endif
#include "threadpool/AIObjectQueue.h"
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdlib>

namespace { constexpr char const* const name_F = "std::function<void()>"; }
#ifdef CWDEBUG
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
#else
using F = std::function<void()>;
#endif

namespace { constexpr char const* const name_B = "Big"; }
#ifdef CWDEBUG
struct B : tracked::Tracked<&name_B> {
  using tracked::Tracked<&name_B>::Tracked;
};
#endif

struct Big {
  char m_data[64] = "Big";
#ifdef CWDEBUG
  B m_tracker;
#endif
};

void f(Big const& DEBUG_ONLY(b))
{
  Dout(dc::notice, "Calling f(\"" << b.m_data << "\")!");
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  //Debug(tracked::mute());

  {
    // Construct an empty buffer.
    Dout(dc::notice|continued_cf, "Constructing AIObjectQueue<" << name_F << "> object_queue... ");
    AIObjectQueue<F> object_queue;
    Dout(dc::finish, "done.");

    ASSERT(object_queue.capacity() == 0);

    int const capacity = 2;

    try {
      // Allocate capacity std::function<void()> objects in it.
      object_queue.reallocate(capacity);
    } catch (std::bad_alloc const& e) {
      std::cout << "Allocation failed: " << e.what() << '\n';
    }

    ASSERT(object_queue.capacity() == capacity);

    auto pa = object_queue.producer_access();
    auto ca = object_queue.consumer_access();

    ASSERT(pa.length() == 0);
    ASSERT(ca.length() == 0);

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

        ASSERT(pa.length() == 1);
        ASSERT(ca.length() == 1);

        {
          Dout(dc::notice|continued_cf, "Moving " << name_F << " out of AIObjectQueue to bf... ");
          F bf(ca.move_out());
          Dout(dc::finish, "Done.");

          ASSERT(pa.length() == 0);
          ASSERT(ca.length() == 0);

          if (!bf)
          {
            Dout(dc::notice, "Buffer empty!");
          }
          else
          {
            Dout(dc::notice|continued_cf, "Copying bf into AIObjectQueue... ");
            pa.move_in(F(bf));
            Dout(dc::finish, "Done.");
            ASSERT(pa.length() == 1);
            ASSERT(ca.length() == 1);
            for (int i = 0; i < 5; ++i)
            {
              Dout(dc::notice|continued_cf, "Moving bf into AIObjectQueue... ");
              pa.move_in(std::move(bf));
              Dout(dc::finish, "Done.");
              ASSERT(pa.length() == 2);
              ASSERT(ca.length() == 2);
              Dout(dc::notice|continued_cf, "Moving " << name_F << " from AIObjectQueue to bf2... ");
              {
                F bf2(ca.move_out());
                Dout(dc::finish, "Done.");
                ASSERT(pa.length() == 1);
                ASSERT(ca.length() == 1);
                Dout(dc::notice|continued_cf, "Executing bf2()...");
                bf2();
                Dout(dc::finish, "Done.");
                Dout(dc::notice|continued_cf, "Moving bf2 to bf");
                bf = std::move(bf2);
                Dout(dc::finish, "Done.");
                Dout(dc::notice|continued_cf, "Destructing bf2... ");
              }
              Dout(dc::finish, "destructed.");
            }
          }

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
