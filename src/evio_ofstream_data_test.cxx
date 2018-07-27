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
#include <fstream>
#include "debug.h"
#include "evio/EventLoopThread.h"
#include "evio/File.h"

using namespace evio;

class MyLinkInputDevice : public LinkInputDevice
{
  using LinkInputDevice::LinkInputDevice;
  // Do thing when read returns zero.
  void read_returned_zero() override { }
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  // Create the thread pool.
  AIThreadPool thread_pool;
  // Create the event loop thread and let it handle new events through the thread pool.
  AIQueueHandle handler = thread_pool.new_queue(32);
  EventLoopThread::instance().init(handler);

  // Open a buffered output file that uses a buffer with a minimum block size of 64 bytes.
  auto& device1 = File<OutputDeviceStream>::create(new OutputBuffer(64), "blah.txt", std::ios_base::trunc);

#if 0
  // Fill the buffer.
  for (int i = 1; i <= 200; ++i)
    device1 << "Hello world " << i << '\n';

  // Start writing the buffer to the file (this returns immediately; the thread pool will do the writing).
  device1.flush();      // This is just ostream::flush().

  // We're done with the device; delete it automatically once all buffered data has been written.
  device1.del();
#endif

  auto* link_buffer = new LinkBuffer(64);
  auto& device2 = File<MyLinkInputDevice>::create(link_buffer, "blah.txt");
  auto& device3 = File<LinkOutputDevice>::create(link_buffer, "blah2.txt");

  // Only write to the file while device2 already has it open.
  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  for (int i = 1; i <= 200; ++i)
    device1 << "Hello world " << i << '\n';
  device1.flush();      // This is just ostream::flush().
  device1.del();

  // Start writing.
  //device3.flush();

  // Clean up when done.
  //device2.del();
  device3.del();

  // Finish active watchers and then return from main loop and join the thread.
  //EventLoopThread::terminate();
  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  Dout(dc::notice, "Leaving main()...");
}
