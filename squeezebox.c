/*
 * squeezebox.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "lmccom.h"
#include "squeezebox.h"
#include "config.h"

cSqueezeConfig cfg;

//***************************************************************************
// Squeeze Control
//***************************************************************************

class cSqueezeControl : public cControl 
{
   public:

      cSqueezeControl();
      virtual ~cSqueezeControl();
      virtual void Hide() {};
      virtual cOsdObject* GetInfo() { return 0; }
      virtual eOSState ProcessKey(eKeys key);

   private:

      cSqueezePlayer* player;
      LmcCom* lmc;
};

cSqueezeControl::cSqueezeControl()
   : cControl(player = new cSqueezePlayer)
{
   lmc = new LmcCom(cfg.mac);
   
   tell(eloAlways, "Trying connetion to '%s:%d', my mac is '%s'", cfg.lmcHost, cfg.lmcPort, cfg.mac);

   if (lmc->open(cfg.lmcHost, cfg.lmcPort) != success)
   {
      tell(eloAlways, "Opening connection to LMC server at '%s:%d' failed", 
           cfg.lmcHost, cfg.lmcPort);
   }
   else
   {
      LmcCom::PlayerInfo* player;
      player = lmc->updatePlayerState();
      tell(0, "Connection to LMC server version '%s' at '%s:%d' established", 
           player->version, cfg.lmcHost, cfg.lmcPort);
   }
}

cSqueezeControl::~cSqueezeControl()
{
   delete lmc;
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSqueezeControl::ProcessKey(eKeys key)
{
   eOSState state = osContinue;

   switch (key)
   {       
      case kStop:
      case kBlue:
      {
         tell(eloAlways, "stopping player");
         Hide();
         player->Stop();
         cControl::Shutdown();
         return osEnd;
      }

      case kPlayPause:
      case kPause:    lmc->pausePlay();  break;

      case kPlay:     lmc->play();       break;
      case kGreen:    lmc->scroll(-10);  break;
      case kYellow:   lmc->scroll(10);   break;
      case kChanUp:   lmc->nextTrack();  break;
      case kChanDn:   lmc->prevTrack();  break;
      case kMute:     lmc->muteToggle(); break;

      case kVolUp:    lmc->volumeUp();   break;
      case kVolDn:    lmc->volumeDown(); break;

      // #TODO - jump direct to track
      // k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,

      default:
         state = osContinue;
   }

   return state;
}

//***************************************************************************
// Squeezebox Plugin
//***************************************************************************

cPluginSqueezebox::cPluginSqueezebox()
{
}

cPluginSqueezebox::~cPluginSqueezebox()
{
}

const char* cPluginSqueezebox::CommandLineHelp()
{
   return 0;
}

bool cPluginSqueezebox::ProcessArgs(int argc, char* argv[])
{
   return true;
}

bool cPluginSqueezebox::Initialize()
{
   return true;
}

bool cPluginSqueezebox::Start()
{
   return true;
}

void cPluginSqueezebox::Stop()
{
}

void cPluginSqueezebox::Housekeeping()
{
}

void cPluginSqueezebox::MainThreadHook()
{
}

cString cPluginSqueezebox::Active()
{
   return 0;
}

time_t cPluginSqueezebox::WakeupTime()
{
   return 0;
}

cOsdObject* cPluginSqueezebox::MainMenuAction()
{
   cControl::Launch(new cSqueezeControl);
   
   return 0;
}

cMenuSetupPage* cPluginSqueezebox::SetupMenu()
{
   return 0;
}

bool cPluginSqueezebox::SetupParse(const char* Name, const char* Value)
{
   return false;
}

bool cPluginSqueezebox::Service(const char* Id, void* Data)
{
   return false;
}

const char** cPluginSqueezebox::SVDRPHelpPages()
{
   return 0;
}

cString cPluginSqueezebox::SVDRPCommand(const char* Command, const char* Option, int &ReplyCode)
{
   return 0;
}

VDRPLUGINCREATOR(cPluginSqueezebox);
