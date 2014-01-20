/*
 * osd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SQUEZZEOSD_H
#define __SQUEZZEOSD_H

#include <vdr/thread.h>

#include "imgtools.h"

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

      cSqueezeOsd(const char* aConfDir);
      virtual ~cSqueezeOsd();

      void view();
      void hide();
      void Action();
      void stop();

      void setForce()                { forceNextDraw = yes; }
      void setButtonLevel(int level) { buttonLevel = level; forceNextDraw = yes; }

      int playlistCount()            { return currentState->plCount; }
      int ProcessKey(int key);

   protected:

      int init();

      int drawOsd();
      int drawCover();
      int drawTrackCover(cPixmap* pixmap, int tid, int x, int y, int size);

      int drawInfoBox();
      int drawProgress(int y = na);
      int drawPlaylist();
      int drawStatus();
      int drawButtons();
      int drawVolume(int x, int y, int width);

      int createBox(cPixmap* pixmap[], int x, int y, int width, int height, 
                    tColor color, tColor blend);

      int drawSymbol(const char* name, int x, int y, int width, int height, cPixmap* pixmap = 0);

      // data

      cOsd* osd;
      LmcCom* lmc;

      int buttonLevel;
      char* confDir;
      int visible;
      int forceNextDraw;
      int loopActive;
      int plCurrent;
      int plUserAction;
      int plTop;
      time_t lastScrollAt;
      
      int plItems;
      int plItemSpace;
      int plItemHeight;

      int symbolBoxHeight;
      int border;                 // border width in pixel

      cImageMagickWrapper* imgLoader;
      LmcCom::PlayerState* currentState;

      cPixmap* pixmapInfo[pmCount];
      cPixmap* pixmapPlaylist[pmCount];
      cPixmap* pixmapPlCurrent[pmCount];
      cPixmap* pixmapStatus[pmCount];

      cPixmap* pixmapBtnRed[pmCount];
      cPixmap* pixmapBtnGreen[pmCount];
      cPixmap* pixmapBtnYellow[pmCount];
      cPixmap* pixmapBtnBlue[pmCount];

      cFont* fontStd;
      cFont* fontTilte;
      cFont* fontArtist;
      cFont* fontPl;

      tColor clrBox;
      tColor clrBoxBlend;
      tColor clrTextDark;
      
      cMyStatus* statusMonitor;
};

//***************************************************************************
#endif // __SQUEZZEOSD_H
