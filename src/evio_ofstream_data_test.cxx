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

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  AIThreadPool thread_pool;
  AIQueueHandle handler = thread_pool.new_queue(32);
  EventLoopThread::instance().init(handler);

  evio::File<evio::OutputDeviceStream>& f(*new evio::File<evio::OutputDeviceStream>);
  AllocTag(&f, "blah.txt");
  f.open("blah.txt");

  for (int i = 1; i <= 128; ++i)
    f << "Hello world " << i << std::endl;

  f.del();		// Get rid of it (after buffered data has been written)

  // Wait until ev_break(EV_A_ EVBREAK_ALL) is called and everything has finished.
  EventLoopThread::instance().join();
}
