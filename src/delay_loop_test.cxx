#include "sys.h"
#include "debug.h"
#include "utils/threading/SpinSemaphore.h"

namespace utils { using namespace threading; }

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  Dout(dc::notice, "Entering main()");
  utils::SpinSemaphore sem1;
  utils::SpinSemaphore sem2;
}
