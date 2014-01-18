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

      cSqueezeOsd();
      virtual ~cSqueezeOsd();

      void view();
      void hide();
      void Action();
      void stop();

      void setForce()          { forceNextDraw = yes; }
      int playlistCount()      { return currentState->plCount; }
      int ProcessKey(int key);

   protected:

      int init();

      int drawOsd();
      int drawCover();
      int drawInfoBox();
      int drawProgress(int y = na);
      int drawPlaylist();
      int drawStatus();
      int drawButtons();

      int createBox(cPixmap* pixmap[], int x, int y, int width, int height, 
                    tColor color, tColor blend);

      // data

      cOsd* osd;
      LmcCom* lmc;

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
      cFont* fontPl;

      tColor clrBox;
      tColor clrBoxBlend;
      tColor clrTextDark;
      
      cMyStatus* statusMonitor;
      int border;    // border width in pixel
};

//***************************************************************************
#endif // __SQUEZZEOSD_H
