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

      char* squeezeCmd;
      char* playerName;
      char* mac;      
      char* audioDevice;
};

extern cSqueezeConfig cfg;
