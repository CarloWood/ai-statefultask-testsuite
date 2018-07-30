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

template<typename DeviceType, typename... ARGS>
boost::intrusive_ptr<DeviceType> create(ARGS&&... args)
{
  return new DeviceType(std::forward<ARGS>(args)...);
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
    auto device1 = create<File<OutputDeviceStream>>(new OutputBuffer(64), "blah.txt", std::ios_base::trunc);
  );

#if 0
  // Fill the buffer.
  for (int i = 1; i <= 200; ++i)
    *device1 << "Hello world " << i << '\n';

  // Start writing the buffer to the file (this returns immediately; the thread pool will do the writing).
  device1->flush();      // This is just ostream::flush().

  // We're done with the device; delete it automatically once all buffered data has been written.
  device1->del();
#endif

  DoutMark("Creating LinkBuffer(64)...",
    auto* link_buffer = new LinkBuffer(64);
  );

  DoutMark("Opening File<LinkInputDevice> \"blah.txt\"...",
    // Passing std::ios_base::app as 'mode' will make the file persistent (not close when it reaches EOF),
    // but instead add a inotify watcher for "blah.txt" and continue reading when something is appended
    // to it.
    auto device2 = create<PersistentInputFile<LinkInputDevice>>(link_buffer, "blah.txt");
  );

  DoutMark("Opening File<LinkOutputDevice> \"blah2.txt\"...",
    auto device3 = create<File<LinkOutputDevice>>(link_buffer, "blah2.txt");
  );

  // Only write to the file while device2 already has it open.
  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  DoutMark("Filling buffer of device1:",
    for (int i = 1; i <= 200; ++i)
      *device1 << "Hello world " << i << '\n';
  );

  DoutMark("Starting device1...",
    device1->flush();      // This is just ostream::flush().
  );

  DoutMark("Calling del() on device1...",
    device1->del();
  );

  // Clean up when done.
  DoutMark("Calling del() on device2...",
    device2->del();
  );
  DoutMark("Calling del() on device3...",
    device3->del();
  );

  Dout(dc::notice, "Sleeping 1 second...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  DoutMark("Calling close() on device2...",
    device2->close();
  );

  // Finish active watchers and then return from main loop and join the thread.
  EventLoopThread::terminate();

  Dout(dc::notice, "Leaving main()...");
}
