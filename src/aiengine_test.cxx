#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>

int main()
{
#ifdef DEBUG_GOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  AIAuxiliaryThread::start();
  for(;;)
  {
    gMainThreadEngine.mainloop();
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}
