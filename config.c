/*
 * config.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include "lib/common.h"
#include "config.h"

//***************************************************************************
// Config
//***************************************************************************

cSqueezeConfig::cSqueezeConfig()
{
   logLevel = 1;
   lmcHost = strdup("localhost");
   lmcPort = 9090;
   lmcHttpPort = 9000;

   squeezeCmd = strdup("/usr/local/bin/squeezelite");
   playerName = strdup("VDR-squeeze");
   audioDevice = strdup("");
   alsaOptions = strdup("");

   shadeTime = 0;
   shadeLevel = 40;  // in %

   mac = getMac();
}

cSqueezeConfig::~cSqueezeConfig()
{
   free(lmcHost);
   free(mac);
   free(squeezeCmd);
   free(playerName);
   free(audioDevice);
   free(alsaOptions);
}

//***************************************************************************
// Setup Menu
//***************************************************************************

cMenuSqueezeSetup::cMenuSqueezeSetup()
{
   strcpy(lmcHost, cfg.lmcHost);
   strcpy(squeezeCmd, cfg.squeezeCmd);
   strcpy(playerName, cfg.playerName);
   strcpy(audioDevice, cfg.audioDevice);
   strcpy(alsaOptions, cfg.alsaOptions);
   strcpy(mac, cfg.mac);

   Setup();
}

void cMenuSqueezeSetup::Setup()
{
   Clear();

   Add(new cMenuEditStrItem(tr("LMS Host"), lmcHost, sizeof(lmcHost), tr(FileNameChars)));
   Add(new cMenuEditIntItem(tr("LMS Port"), &cfg.lmcPort, 1, 99999));
   Add(new cMenuEditIntItem(tr("LMS/Http Port"), &cfg.lmcHttpPort, 1, 99999));

   Add(new cMenuEditStrItem(tr("Player Name"), playerName, sizeof(playerName), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("Player MAC"), mac, sizeof(mac), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("Squeezelite Path"), squeezeCmd, sizeof(squeezeCmd), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("Audio Device"), audioDevice, sizeof(audioDevice), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("Alsa Options"), alsaOptions, sizeof(alsaOptions), tr(FileNameChars)));

   Add(new cMenuEditIntItem(tr("Shade Time [s]"), &cfg.shadeTime, 0, 3600));
   Add(new cMenuEditIntItem(tr("Shade Level [%]"), &cfg.shadeLevel, 0, 100));
   Add(new cMenuEditBoolItem(tr("Rounded OSD"), &cfg.rounded));

   Add(new cMenuEditIntItem(tr("Log level"), &cfg.logLevel, 0, 4));

//   SetCurrent(Get(current));
   Display();
}

eOSState cMenuSqueezeSetup::ProcessKey(eKeys Key)
{
   eOSState state = cMenuSetupPage::ProcessKey(Key);

   switch (state)
   {
      case osContinue:
      {
         if (NORMALKEY(Key) == kUp || NORMALKEY(Key) == kDown)
         {
            cOsdItem* item = Get(Current());

            if (item)
               item->ProcessKey(kNone);
         }

         break;
      }

      default: break;
   }

   return state;
}

void cMenuSqueezeSetup::Store(void)
{
   free(cfg.lmcHost);     cfg.lmcHost = strdup(lmcHost);
   free(cfg.squeezeCmd);  cfg.squeezeCmd = strdup(squeezeCmd);
   free(cfg.playerName);  cfg.playerName = strdup(playerName);
   free(cfg.mac);         cfg.mac = strdup(mac);
   free(cfg.audioDevice); cfg.audioDevice = strdup(audioDevice);
   free(cfg.alsaOptions); cfg.alsaOptions = strdup(alsaOptions);

   SetupStore("logLevel", cfg.logLevel);
   SetupStore("lmcHost", cfg.lmcHost);
   SetupStore("lmcPort", cfg.lmcPort);
   SetupStore("lmcHttpPort", cfg.lmcHttpPort);
   SetupStore("rounded", cfg.rounded);
   SetupStore("shadeTime", cfg.shadeTime);
   SetupStore("shadeLevel", cfg.shadeLevel);
   SetupStore("squeezeCmd", cfg.squeezeCmd);
   SetupStore("playerName", cfg.playerName);
   SetupStore("playerMac", cfg.mac);
   SetupStore("audioDevice", cfg.audioDevice);
   SetupStore("alsaOptions", cfg.alsaOptions);
}
