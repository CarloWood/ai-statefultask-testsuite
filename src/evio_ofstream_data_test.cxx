// Copyright (C) 2004, by
//
// Carlo Wood, Run on IRC <carlo@alinoe.com>
// RSA-1024 0x624ACAD5 1997-01-26                    Sign & Encrypt
// Fingerprint16 = 32 EC A7 B6 AC DB 65 A6  F6 F6 55 DD 1C DC FF 61
//
// This file may be distributed under the terms of the Q Public License
// version 1.0 as appearing in the file LICENSE.QPL included in the
// packaging of this file.
//

#include "sys.h"
#include "debug.h"
#include "evio/EventLoopThread.h"
#include "evio/File.h"
#include "evio/INotify.h"
#include <fstream>

using namespace evio;

class MyLinkInputDevice;

class MyINotify : public INotify
{
  MyLinkInputDevice* m_link_input_device;

 public:
  MyINotify(MyLinkInputDevice* link_input_device) : m_link_input_device(link_input_device) { }
#ifdef CWDEBUG
  ~MyINotify()
  {
    Dout(dc::notice, "Calling ~MyINotify()");
    // Make sure we don't use an uninitialized pointer without noticing it.
    m_link_input_device = nullptr;
  }
#endif

 protected:
  void event_occurred(inotify_event const* event) override;
};

class MyLinkInputDevice : public LinkInputDevice
{
  MyINotify m_inotify;

  friend File<MyLinkInputDevice>;
  MyLinkInputDevice(LinkBuffer* lbuf) : LinkInputDevice(lbuf), m_inotify(this) { }

  // Do things when read returns zero.
  void read_returned_zero() override;
  void closed() override;
  friend MyINotify;
  void in_modify() { start_input_device(); }
};

void MyLinkInputDevice::read_returned_zero()
{
  DoutEntering(dc::evio, "MyLinkInputDevice::read_returned_zero()");
  stop_input_device();
  if (m_inotify.is_watched())   // Already watched?
    return;
  // Add an inotify watch for modification of the corresponding path.
  File<MyLinkInputDevice>* file_device = dynamic_cast<File<MyLinkInputDevice>*>(this);
  // This should always be the case because File<MyLinkInputDevice> is the only one that can use MyLinkInputDevice!
  ASSERT(file_device);
  if (file_device && !file_device->open_filename().empty())
  {
    if (m_inotify.add_watch(file_device->open_filename().c_str(), IN_MODIFY))
    {
      Dout(dc::evio, "Incrementing ref count of this device [" << (void*)static_cast<IOBase*>(this) << ']');
      intrusive_ptr_add_ref(this);      // Keep this object alive because the above call registered m_inotify as callback object.
    }
  }
}

void MyLinkInputDevice::closed()
{
  DoutEntering(dc::evio, "MyLinkInputDevice::closed()");
  if (m_inotify.is_watched())
  {
    m_inotify.rm_watch();
    Dout(dc::evio, "Decrementing ref count of this device [" << (void*)static_cast<IOBase*>(this) << ']');
    intrusive_ptr_release(this);
  }
}

void MyINotify::event_occurred(inotify_event const* event)
{
  if ((event->mask & IN_MODIFY))
    m_link_input_device->in_modify();
}

#define DoutMark(text, statements...) \
  Dout(dc::notice, text); \
  libcwd::libcw_do.push_marker(); \
  libcwd::libcw_do.marker().append("| "); \
  statements; \
  libcwd::libcw_do.pop_marker()

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  // Create the thread pool.
  AIThreadPool thread_pool;
  // Create the event loop thread and let it handle new events through the thread pool.
  AIQueueHandle handler = thread_pool.new_queue(32);
  EventLoopThread::instance().init(handler);

  // Open a buffered output file that uses a buffer with a minimum block size of 64 bytes.
  DoutMark("Opening File<OutputDeviceStream> \"blah.txt\" (device1)...",
    auto& device1 = File<OutputDeviceStream>::create(new OutputBuffer(64), "blah.txt", std::ios_base::trunc);
  );

#if 0
  // Fill the buffer.
  for (int i = 1; i <= 200; ++i)
    device1 << "Hello world " << i << '\n';

  // Start writing the buffer to the file (this returns immediately; the thread pool will do the writing).
  device1.flush();      // This is just ostream::flush().

  // We're done with the device; delete it automatically once all buffered data has been written.
  device1.del();
#endif

  DoutMark("Creating LinkBuffer(64)...",
    auto* link_buffer = new LinkBuffer(64);
  );

  DoutMark("Opening File<MyLinkInputDevice> \"blah.txt\"...",
    auto& device2 = File<MyLinkInputDevice>::create(link_buffer, "blah.txt");
  );

  DoutMark("Opening File<LinkOutputDevice> \"blah2.txt\"...",
    auto& device3 = File<LinkOutputDevice>::create(link_buffer, "blah2.txt");
  );

  // Only write to the file while device2 already has it open.
  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  DoutMark("Filling buffer of device1:",
    for (int i = 1; i <= 200; ++i)
      device1 << "Hello world " << i << '\n';
  );

  DoutMark("Starting device1...",
    device1.flush();      // This is just ostream::flush().
  );

  DoutMark("Calling del() on device1...",
    device1.del();
  );

  // Clean up when done.
  DoutMark("Calling del() on device2...",
    device2.del();
  );
  DoutMark("Calling del() on device3...",
    device3.del();
  );

  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  DoutMark("Calling close() on device2...",
    device2.close();
  );

  // Finish active watchers and then return from main loop and join the thread.
  EventLoopThread::terminate();

  Dout(dc::notice, "Leaving main()...");
}
