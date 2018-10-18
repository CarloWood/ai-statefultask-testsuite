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
#include "evio/PersistentInputFile.h"

using namespace evio;

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  // Create the thread pool.
  AIThreadPool thread_pool;
  // Create the event loop thread and let it handle new events through the thread pool.
  AIQueueHandle handler = thread_pool.new_queue(32);
  EventLoopThread::instance().init(handler);

  {
    using device2_type = PersistentInputFile<LinkInputDevice>;
    boost::intrusive_ptr<device2_type> device2;
    {
      // Open a buffered output file that uses a buffer with a minimum block size of 64 bytes.
      auto out_buffer = new OutputBuffer(64);
      auto device1 = create<File<OutputDeviceStream>>(out_buffer, "blah.txt", std::ios_base::trunc);

      // Open an input file that reads 'persistent' (see device2_type above) from blah.txt
      // and link with an output file that writes to blah2.txt.
      auto* link_buffer = new LinkBuffer(64);
      device2 = create<device2_type>(link_buffer, "blah.txt");
      auto device3 = create<File<LinkOutputDevice>>(link_buffer, "blah2.txt");

      // Give device2 time to read till EOF. This tests the persistent-ness. If it wasn't
      // persistent it would close itself when reaching EOF and nothing would be written
      // to blah2.txt.
      Dout(dc::notice, "Sleeping 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));

      // Fill buffer of device1 with data.
      Dout(dc::notice|continued_cf, "Fill buffer of device1 with data... ");
      Debug(dc::evio.off());
      for (int i = 1; i <= 200; ++i)
        *device1 << "Hello world " << i << '\n';
      Debug(dc::evio.on());
      Dout(dc::finish, "done");
      // Actually (start) writing data to device1.
      device1->flush();      // This is just ostream::flush().

      // Destruct device pointers device1 and device3 - that tests that they aren't deleted before they are finished.
    }

    // Wait with closing device2 so it has time to read whatever is written to blah.txt.
    Dout(dc::notice, "Sleeping 1 second...");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Actually get rid of device2 (it won't disappear otherwise, since it is persistent).
    device2->close();             // This causes device2 to be closed and deleted (and therefore (soonish?) device3 to also be deleted).
    // Destruct device pointer device2 too (everything has already completed here though).
  }

  // Finish active watchers and then return from main loop and join the thread.
  EventLoopThread::terminate();
  Dout(dc::notice, "Leaving main()...");
}
