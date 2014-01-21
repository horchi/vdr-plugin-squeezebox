/*
 * squeezebox.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef __SQUEEZEBOX_H
#define __SQUEEZEBOX_H

#include <vdr/plugin.h>
#include <vdr/player.h>
#include <vdr/thread.h>
#include <vdr/remote.h>

#include "lmccom.h"

static const char *VERSION        = "0.0.5";
static const char *DESCRIPTION    = "Squeezebox  - a client for the Logitech Media Server";
static const char *MAINMENUENTRY  = "Squeezebox";

//***************************************************************************
// cSqueezePlayer
//***************************************************************************

class cSqueezePlayer : public cPlayer, cThread 
{
   public:

      cSqueezePlayer();
      virtual ~cSqueezePlayer();
      
      virtual void Stop();

      int started() { return running; }

   protected:

      virtual void Activate(bool on);
      virtual void Action();
      virtual int startPlayer();
      virtual int stopPlayer();
      
      int running;
      pid_t pid;
};

//***************************************************************************
// Plugin Main Menu
//***************************************************************************

class cSqueezeMenu : public cOsdMenu
{
   public:

      cSqueezeMenu(const char* title, LmcCom* aLmc);
      virtual ~cSqueezeMenu() { };
      
      virtual eOSState ProcessKey(eKeys key);

   protected:

      LmcCom* lmc;
};

//***************************************************************************
// Plugin 
//***************************************************************************

class cPluginSqueezebox : public cPlugin 
{
   public:

      cPluginSqueezebox();
      virtual ~cPluginSqueezebox();

      virtual const char* Version(void)     { return VERSION; }
      virtual const char* Description(void) { return DESCRIPTION; }

      void activateMenu(LmcCom* aLmc) 
      { 
         doActivateMenu = yes; 
         lmcForMenu = aLmc;
         
         if (lmcForMenu)
            cRemote::CallPlugin("squeezebox");
         else
            tell(0, "Missing lmc control handle");
      }

      virtual const char* CommandLineHelp();
      virtual bool ProcessArgs(int argc, char *argv[]);
      virtual bool Initialize(void);
      virtual bool Start(void);
      virtual void Stop(void);
      virtual void Housekeeping(void);
      virtual void MainThreadHook(void);
      virtual cString Active(void);
      virtual time_t WakeupTime(void);
      virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
      virtual cOsdObject *MainMenuAction(void);
      virtual cMenuSetupPage *SetupMenu(void);
      virtual bool SetupParse(const char *Name, const char *Value);
      virtual bool Service(const char *Id, void *Data = NULL);
      virtual const char **SVDRPHelpPages(void);
      virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);

   protected:

      // data

      LmcCom* lmcForMenu;
      int doActivateMenu;
};

//***************************************************************************
#endif //  __SQUEEZEBOX_H
