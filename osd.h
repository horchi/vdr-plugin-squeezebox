/*
 * osd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SQUEZZEOSD_H
#define __SQUEZZEOSD_H

#include <vdr/thread.h>
#include <vdr/plugin.h>

#include "lmccom.h"
#include "imgtools.h"
#include "menu.h"

class cMyStatus;

//***************************************************************************
// Osd
//***************************************************************************

class cSqueezeOsd : public cThread
{
   public:

      enum Pixmap
      {
         pmBack,
         pmText,
         pmCount
      };

      cSqueezeOsd(const char* aResDir = "");
      virtual ~cSqueezeOsd();

      int init();
      int exit();

      void view();
      void hide();
      void Action();
      void stop();

      void setForce()                { forceNextDraw = yes; }
      void setButtonLevel(int level) { buttonLevel = level; forceNextDraw = yes; }

      int playlistCount()            { return currentState->plCount; }
      int ProcessKey(int key);

      int activateMenu(LmcCom* aLmc)
      {
         if (!menu)
         {
           menu = new cMenuSqueeze("Squeezebox", aLmc);
           menu->setVisibleItems(visibleMenuItems);
           forceMenuDraw = yes;
         }

         return done;
      }

      void setActivity() { lastActivityAt = time(0); }

   protected:

      // osd2web

      int sendInfoBox(TrackInfo* currentTrack);

      // draw

      int drawOsd();
      int drawCover();
      int drawTrackCover(cPixmap* pixmap, TrackInfo* track, int x, int y, int size);

      int drawInfoBox();
      int drawProgress(int y = na);
      int drawPlaylist();
      int drawStatus();
      int drawButtons();
      int drawVolume(cPixmap* pixmap, int x, int y, int width);
      int drawMenu();
      int scrollLyrics();

      int createBox(cPixmap* pixmap[], int x, int y, int width, int height,
                    tColor color, tColor blend, int radius);

      int drawSymbol(cPixmap* pixmap, const char* name, int& x, int y, int width = na, int height = na);

      const char* Red()
      {
         if (menu)
            return menu->getActive()->Red();

         return buttonLevel == 0 ? tr("Menu") : tr("Shuffle");
      }
      const char* Green()
      {
         if (menu)
            return menu->getActive()->Green();
         return buttonLevel == 0 ? tr("<<") : tr("Repeat");
      }
      const char* Yellow()
      {
         if (menu)
            return menu->getActive()->Yellow();
         return  buttonLevel == 0 ? tr(">>") : tr("Vol-");
      }
      const char* Blue()
      {
         if (menu)
            return menu->getActive()->Blue();
         return buttonLevel == 0 ? (currentState->plCount ? tr("Clear") : tr("Random")) : tr("Vol+");
      }

      // data

      cOsd* osd;
      LmcCom* lmc;
      cMenuBase* menu;

      int buttonLevel;
      char* resDir;
      int forceNextDraw;
      int forceMenuDraw;
      int forcePlaylistDraw;
      int loopActive;
      int plCurrent;
      int plUserAction;
      int plTop;
      time_t lastScrollAt;

      int plItems;
      int plItemSpace;
      int plItemHeight;
      int menuItemHeight;
      int menuTop;
      int menuItems;
      int menuItemSpace;
      int visibleMenuItems;
      int border;                 // border width in pixel

      cImageMagickWrapper* imgLoader;
      PlayerState* currentState;
      cPlugin* osd2web;

      cPixmap* pixmapCover[pmCount];
      cPixmap* pixmapInfo[pmCount];
      cPixmap* pixmapPlaylist[pmCount];
      cPixmap* pixmapPlCurrent[pmCount];
      cPixmap* pixmapSymbols[pmCount];
      cPixmap* pixmapStatus[pmCount];

      cPixmap* pixmapBtnRed[pmCount];
      cPixmap* pixmapBtnGreen[pmCount];
      cPixmap* pixmapBtnYellow[pmCount];
      cPixmap* pixmapBtnBlue[pmCount];

      cPixmap* pixmapMenuTitle[pmCount];
      cPixmap* pixmapMenu[pmCount];
      cPixmap* pixmapMenuCurrent[pmCount];
      cPixmap* pixmapLyrics;

      int lyricsScrollPos;
      int lyricsDrawportHeight;
      int lyricsScrollingAt;
      uint64_t nextScrollStep;
      char lastLyrics[200];

      cFont* fontStd;
      cFont* fontTilte;
      cFont* fontArtist;
      cFont* fontPl;
      cFont* fontLyrics;

      tColor clrBox;
      tColor clrBoxBlend;
      tColor clrTextDark;

      unsigned short alpha;
      time_t lastActivityAt;

      cMyStatus* statusMonitor;
};

//***************************************************************************
#endif // __SQUEZZEOSD_H
