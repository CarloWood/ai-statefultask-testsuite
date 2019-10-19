#include "sys.h"
#include "debug.h"
#include "threadsafe/SpinSemaphore.h"

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  Dout(dc::notice, "Entering main()");
  aithreadsafe::SpinSemaphore sem1;
  aithreadsafe::SpinSemaphore sem2;
}
