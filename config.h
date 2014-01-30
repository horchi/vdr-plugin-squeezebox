/*
 * config.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

//***************************************************************************
// 
//***************************************************************************

class cSqueezeConfig
{
   public:

      cSqueezeConfig();
      ~cSqueezeConfig();

      // config data

      char* lmcHost;
      unsigned short lmcPort;
      unsigned short lmcHttpPort;

      char* squeezeCmd;
      char* playerName;
      char* mac;
      char* audioDevice;
      unsigned short logLevel;
};

extern cSqueezeConfig cfg;
