#include "sys.h"
#include "evio/FileDescriptor.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"

using namespace evio;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle high_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle medium_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle low_priority_handler = thread_pool.new_queue(16);

  // Create the IO event loop thread.
  EventLoopThread evio_loop(low_priority_handler);

  FileDescriptor* fdp0 = new FileDescriptor;
  FileDescriptor* fdp1 = new FileDescriptor;

  fdp0->init(0, evio::READ);
  fdp1->init(1, evio::WRITE);

  fdp0->start(evio_loop);
  fdp1->start(evio_loop);
}
