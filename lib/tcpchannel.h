
//***************************************************************************
// Group VDR/GraphTFT
// File tcpchannel.h
// Date 31.10.06
// This code is distributed under the terms and conditions of the
// GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
// (c) 2006-2008 J�rg Wendel
//--------------------------------------------------------------------------
// Class TcpChannel
//***************************************************************************

#ifndef __GTFT_TCPCHANNEL_H__
#define __GTFT_TCPCHANNEL_H__

#include <vdr/thread.h>

#include "common.h"

//***************************************************************************
// Class TcpChannel
//***************************************************************************

class TcpChannel
{
   public:	

     enum Errors
      {
         errChannel = -100,

         errUnknownHostname,      // 99
         errBindAddressFailed,    // 98
         errAcceptFailed,
         errListenFailed,
         errConnectFailed,        // 95
         errIOError,              // 94
         errConnectionClosed,     // 93
         errInvalidEndpoint,
         errOpenEndpointFailed,   // 91

         // Warnungen

         wrnNoEventPending,       // 90
         errUnexpectedEvent,      // 89
         wrnChannelBlocked,       // 88
         wrnNoConnectIndication,  // 87
         wrnNoResponseFromServer, // 86
         wrnNoDataAvaileble,      // 85
         wrnSysInterrupt,         // 84
         wrnTimeout               // 83
      };

#pragma pack(1)
      struct Header
      {
         int command;
         int size;
      };
#pragma pack()

		TcpChannel(int aTimeout = 30, int aHandle = 0);
		~TcpChannel();

      int openLstn(unsigned short aPort, const char* aLocalHost = 0);
      int open(unsigned short aPort, const char* aHost);
      int isOpen() { return handle > 0; }
      int flush();
      int close();
		int listen(TcpChannel*& child);
      int look(uint64_t aTimeout = 0);
      int read(char* buf, int bufLen, int ln = no);
      char* readln();

      int writeCmd(int command, const char* buf = 0, int bufLen = 0);
      int write(const char* buf, int bufLen = 0);

      int isConnected()    { return handle != 0; }
      int getHandle()      { return handle; }

   private:

      int checkErrno();

      // data

      int handle;
      unsigned short port;
      char localHost[100];
      char remoteHost[100];
      long localAddr;
      long remoteAddr;
      long timeout;
      int lookAheadChar;
      int lookAhead;
      int nTtlReceived;
      int nTtlSent;

      char* readBuffer;
      int readBufferSize;
      int readBufferPending;

#ifdef VDR_PLUGIN
      cMutex _mutex;
#endif
};

//***************************************************************************
#endif // __GTFT_TCPCHANNEL_H__
