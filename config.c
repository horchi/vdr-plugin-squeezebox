
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
   lmcHost = strdup("localhost");
   lmcPort = 9090;

   squeezeCmd = strdup("/usr/local/bin/squeezelite");
   playerName = strdup("VDR-squeeze");

   // audioDevice = strdup("hw:CARD=NVidia,DEV=9");
   audioDevice = strdup("");

   mac = getMac();
}

cSqueezeConfig::~cSqueezeConfig()
{
   free(lmcHost);
   free(mac);
   free(squeezeCmd);
   free(playerName);
   free(audioDevice);
}
