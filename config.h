/*
 * config.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/menuitems.h>

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
      int lmcPort;
      int lmcHttpPort;

      char* squeezeCmd;
      char* playerName;
      char* mac;
      char* audioDevice;
      char* alsaOptions;

      int logLevel;
      int shadeTime;
      int shadeLevel;
      int rounded = no;
};

extern cSqueezeConfig cfg;

//***************************************************************************
// Plugin Setup Menu
//***************************************************************************

class cMenuSqueezeSetup : public cMenuSetupPage
{
   public:

      cMenuSqueezeSetup();

   protected:

      virtual eOSState ProcessKey(eKeys Key);
      virtual void Store();
      virtual void Setup();

      char lmcHost[256];
      char squeezeCmd[256];
      char playerName[20];
      char audioDevice[30];
      char alsaOptions[30];
      char mac[20];
};
