#include "sys.h"
#include "debug.h"
#include "statefultask/AIDelayedFunction.h"
#include <iostream>

void g(int a, int b)
{
  std::cout << "Calling g(" << a << ", " << b << ")\n";
}

struct A {
  void g(int a, int b)
  {
    std::cout << "Calling A::g(" << a << ", " << b << ")\n";
  }
};

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  A foo;
  AIDelayedFunction<int, int> f1(&foo, &A::g);
  AIDelayedFunction<int, int> f2(&g);
  AIDelayedFunction<int, int> f3([](int x, int y){ std::cout << "Calling lambda(" << x << ", " << y << ")\n"; });

  // Each of these calls takes about 0.1 microsecond.
  f1(1, 2);
  f2(3, 4);
  f3(5, 6);
  
  std::cout << "Before calling f.invoke().\n";
  f3.invoke();
  f2.invoke();
  f1.invoke();

  // Output:
  //
  // Before calling f.invoke().
  // Calling lambda(5, 6)
  // Calling g(3, 4)
  // Calling A::g(1, 2)
}
