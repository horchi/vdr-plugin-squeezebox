/*
 * squeezebox.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "lmccom.h"
#include "squeezebox.h"
#include "config.h"
#include "osd.h"

#include "lib/common.h"

cSqueezeConfig cfg;

//***************************************************************************
// Squeeze Control
//***************************************************************************

class cSqueezeControl : public cControl 
{
   public:

      cSqueezeControl(cPluginSqueezebox* aPlugin, const char* aResDir);
      virtual ~cSqueezeControl();

      virtual void Hide() { osdThread->hide(); };
      virtual cOsdObject* GetInfo() { return 0; }
      virtual eOSState ProcessKey(eKeys key);

   private:

      char* resDir;
      cSqueezePlayer* player;
      LmcCom* lmc;
      cSqueezeOsd* osdThread;
      cPluginSqueezebox* plugin;
      int startDone;
      int buttonLevel;
};

//***************************************************************************
// Squeeze Control
//***************************************************************************

cSqueezeControl::cSqueezeControl(cPluginSqueezebox* aPlugin, const char* aResDir)
   : cControl(player = new cSqueezePlayer)
{
   plugin = aPlugin;
  
   buttonLevel = 0; 
   resDir = strdup(aResDir);
   startDone = no;
   lmc = new LmcCom(cfg.mac);
   osdThread = new cSqueezeOsd(resDir);
   osdThread->Start();

   tell(eloAlways, "Trying connetion to '%s:%d', my mac is '%s'", cfg.lmcHost, cfg.lmcPort, cfg.mac);

   if (lmc->open(cfg.lmcHost, cfg.lmcPort) != success)
   {
      tell(eloAlways, "Opening connection to LMC server at '%s:%d' failed", 
           cfg.lmcHost, cfg.lmcPort);
   }
   else
   {
      tell(eloAlways, "Connection to LMC server at '%s:%d' established", 
           cfg.lmcHost, cfg.lmcPort);
   }
}

cSqueezeControl::~cSqueezeControl()
{
   // lmc->save();

   free(resDir);
   delete lmc;
   delete osdThread;
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSqueezeControl::ProcessKey(eKeys key)
{
   eOSState state = osContinue; // cControl::ProcessKey(key);

   if (!startDone && player->started())
   {
      startDone = yes;
      // lmc->resume();
   }

   // check if osd handle this key ...

   if (key != kNone && osdThread->ProcessKey(key) == done)
      return osContinue;

   if (key > k0 && key <= k9)
   {
      lmc->track(key - k0);

      return osContinue;
   }

   switch ((int)key)   // cast to avoid warnings
   {       
      case kBack:
      {
         tell(eloAlways, "Stopping player");
         osdThread->hide();
         player->Stop();
         cControl::Shutdown();
         return osEnd;
      }

      case kStop:     lmc->stop();       break;
      case kPlayPause:
      case kPause:    lmc->pausePlay();  break;
      case kPlay:     lmc->play();       break;
      case kFastRew|k_Repeat:
      case kFastRew:  lmc->scroll(-10);  break;
      case kFastFwd|k_Repeat:
      case kFastFwd:  lmc->scroll(10);   break;
      case kNext|k_Repeat:
      case kNext:
      case kChanUp|k_Repeat:
      case kChanUp:   lmc->nextTrack();  break;
      case kPrev:
      case kPrev|k_Repeat:
      case kChanDn|k_Repeat:
      case kChanDn:   lmc->prevTrack();  break;
      case kMute:     lmc->muteToggle(); break;

      case k0:
      {
         buttonLevel = buttonLevel == 0 ? 1 : 0;
         osdThread->setButtonLevel(buttonLevel);
         break;
      }
      
      case kRed:
      {
         if (buttonLevel == 0)
            osdThread->activateMenu(lmc);
         else
            lmc->shuffle();

         break;
      }

      case kGreen:
      {
         if (buttonLevel == 0)
            lmc->scroll(-10);
         else
            lmc->repeat();

         break;
      }

      case kYellow|k_Repeat:
      case kYellow:
      {
         if (buttonLevel == 0)
            lmc->scroll(10);
         else
            lmc->volumeDown();
         break;
      }

      case kBlue|k_Repeat:
      case kBlue:
      {
         if (buttonLevel == 0)
         {
            if (osdThread->playlistCount())
               lmc->clear();
            else
               lmc->randomTracks();
         }
         else
            lmc->volumeUp();

         break;
      }

      default:
         return cControl::ProcessKey(key);
   }

   return state;
}

//***************************************************************************
// - PLUGIN - 
//***************************************************************************
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
   loglevel = cfg.logLevel;
   cControl::Launch(new cSqueezeControl(this, ResourceDirectory()));

   return 0;
}

cMenuSetupPage* cPluginSqueezebox::SetupMenu()
{
   return 0;
}

bool cPluginSqueezebox::SetupParse(const char* Name, const char* Value)
{
  if      (!strcasecmp(Name, "logLevel"))     cfg.logLevel = atoi(Value);
  else if (!strcasecmp(Name, "lmcPort"))      cfg.lmcPort = atoi(Value);
  else if (!strcasecmp(Name, "lmcHttpPort"))  cfg.lmcHttpPort = atoi(Value);

  else if (!strcasecmp(Name, "lmcHost"))      { free(cfg.lmcHost);     cfg.lmcHost = strdup(Value); }
  else if (!strcasecmp(Name, "squeezeCmd"))   { free(cfg.squeezeCmd);  cfg.squeezeCmd = strdup(Value); }
  else if (!strcasecmp(Name, "playerName"))   { free(cfg.playerName);  cfg.playerName = strdup(Value); }
  else if (!strcasecmp(Name, "playerMac"))    { free(cfg.mac);         cfg.mac = strdup(Value); }
  else if (!strcasecmp(Name, "audioDevice"))  { free(cfg.audioDevice); cfg.audioDevice = strdup(Value); }

  else
     return false;

  return true;
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

VDRPLUGINCREATOR(cPluginSqueezebox)
