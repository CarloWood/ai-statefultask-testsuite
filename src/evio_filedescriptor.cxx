#include "sys.h"
#include "evio/Device.h"
#include "debug.h"
#include "statefultask/AIThreadPool.h"
#include "evio/EventLoopThread.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

using namespace evio;

class TestInputDevice : public InputDevice
{
 public:
  TestInputDevice() : InputDevice(new evio::InputBuffer) { }

 protected:
  void read_from_fd(int fd) override;   // Read thread.
};

class TestOutputDevice : public OutputDevice
{
 public:
  TestOutputDevice() : OutputDevice(new evio::OutputBuffer) { }

 protected:
  void write_to_fd(int fd) override;    // Write thread.
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  [[maybe_unused]] AIQueueHandle high_priority_handler = thread_pool.new_queue(32);
  [[maybe_unused]] AIQueueHandle medium_priority_handler = thread_pool.new_queue(32);
  AIQueueHandle low_priority_handler = thread_pool.new_queue(16);

  // Create the IO event loop thread.
  EventLoopThread::instance().init(low_priority_handler);

  InputDevice* fdp0 = new TestInputDevice;
  OutputDevice* fdp1 = new TestOutputDevice;

  fdp0->init(0);        // Standard input.
  fdp1->init(1);        // Standard output.

  fdp0->start();
  fdp1->start();

  // Wait until all watchers have finished.
  EventLoopThread::terminate();
}

// Read thread.
void TestInputDevice::read_from_fd(int fd)
{
  DoutEntering(dc::notice, "TestInputDevice::read_from_fd(" << fd << ")");

  char buf[256];
  ssize_t len;
  do
  {
    len = read(fd, buf, 256);
    Dout(dc::notice, "Read: \"" << libcwd::buf2str(buf, len) << "\".");
  }
  while (len == 256);

  if (strncmp(buf, "quit\n", 5) == 0)
  {
    ev_break(EV_A_ EVBREAK_ALL);        // Terminate EventLoopThread.
    close();                            // Remove this object.
  }
}

// Write thread.
void TestOutputDevice::write_to_fd(int fd)
{
  DoutEntering(dc::notice, "TestOutputDevice::write_to_fd(" << fd << ")");
  [[maybe_unused]] int unused = write(fd, "Hello World\n", 12);
  stop_output_device();
}
