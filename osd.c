/*
 * osd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include <vdr/keys.h>
#include <vdr/status.h>

#include "lib/common.h"

#include "lmccom.h"
#include "config.h"
#include "osd.h"
#include "helpers.h"

//***************************************************************************
// Status Interface
//***************************************************************************

class cMyStatus : public cStatus
{
   public:

      cMyStatus(cSqueezeOsd* o) : cStatus()  { osd = o; }

   protected:
      
      virtual void OsdClear(void)                       { osd->setForce(); }
      virtual void SetVolume(int Volume, bool Absolute) { osd->setForce(); }
      
   private:
      
      cSqueezeOsd* osd;
};
   
//***************************************************************************
// Squeeze Osd
//***************************************************************************

cSqueezeOsd::cSqueezeOsd(const char* aResDir)
   : cThread()
{
   loopActive = no;
   visible = no;
   forceNextDraw = yes;
   forceMenuDraw = no;
   border = 10;
   plTop = 0;
   plCurrent = 0;              // current user selected item, or current track (if no user action)
   plUserAction = no;          // indicates that user scrolling the list
   lastScrollAt = time(0);
   plItemSpace = 10;
   plItems = 0;
   plItemHeight = 0;
   resDir = strdup(aResDir);
   symbolBoxHeight = 100;
   buttonLevel = 0;
   menu = 0;
   menuTop = 0;
   visibleMenuItems = 0;
   menuItemHeight = 0;
   menuItemSpace = 5;

   statusMonitor = new cMyStatus(this);

   osd = 0;

   memset(pixmapInfo, 0, sizeof(pixmapInfo));
   memset(pixmapPlaylist, 0, sizeof(pixmapPlaylist));
   memset(pixmapPlCurrent, 0, sizeof(pixmapPlCurrent));
   memset(pixmapStatus, 0, sizeof(pixmapStatus));
   memset(pixmapBtnRed, 0, sizeof(pixmapBtnRed));
   memset(pixmapBtnGreen, 0, sizeof(pixmapBtnGreen));
   memset(pixmapBtnYellow, 0, sizeof(pixmapBtnYellow));
   memset(pixmapBtnBlue, 0, sizeof(pixmapBtnBlue));
   memset(pixmapMenuTitle, 0, sizeof(pixmapMenuTitle));
   memset(pixmapMenu, 0, sizeof(pixmapMenu));
   memset(pixmapMenuCurrent, 0, sizeof(pixmapMenuCurrent));

   fontStd = 0;
   fontTilte = 0;
   fontArtist = 0;
   fontPl = 0;

   clrBox = 0xFF000040;
   clrBoxBlend = 0xFF0000AA;
   clrTextDark = 0xFF808080;

   lmc = new LmcCom(cfg.mac);
   imgLoader = new cImageMagickWrapper();
   
   if (lmc->open(cfg.lmcHost, cfg.lmcPort) != success)
   {
      tell(eloAlways, "Opening connection to LMC server at '%s:%d' failed", 
           cfg.lmcHost, cfg.lmcPort);
   }

   init();
}

cSqueezeOsd::~cSqueezeOsd()
{
   stop();

   if (osd)
   {
      osd->DestroyPixmap(pixmapInfo[0]);
      osd->DestroyPixmap(pixmapInfo[1]);
      osd->DestroyPixmap(pixmapPlaylist[0]);
      osd->DestroyPixmap(pixmapPlaylist[1]);
      osd->DestroyPixmap(pixmapPlCurrent[0]);
      osd->DestroyPixmap(pixmapPlCurrent[1]);
      osd->DestroyPixmap(pixmapStatus[0]);
      osd->DestroyPixmap(pixmapStatus[1]);
      
      osd->DestroyPixmap(pixmapBtnRed[0]);
      osd->DestroyPixmap(pixmapBtnRed[1]);
      osd->DestroyPixmap(pixmapBtnGreen[0]);
      osd->DestroyPixmap(pixmapBtnGreen[1]);
      osd->DestroyPixmap(pixmapBtnYellow[0]);
      osd->DestroyPixmap(pixmapBtnYellow[1]);
      osd->DestroyPixmap(pixmapBtnBlue[0]);
      osd->DestroyPixmap(pixmapBtnBlue[1]);

      osd->DestroyPixmap(pixmapMenuTitle[0]);
      osd->DestroyPixmap(pixmapMenuTitle[1]);
      osd->DestroyPixmap(pixmapMenu[0]);
      osd->DestroyPixmap(pixmapMenu[1]);
      osd->DestroyPixmap(pixmapMenuCurrent[0]);
      osd->DestroyPixmap(pixmapMenuCurrent[1]);
   }

   delete fontTilte;
   delete fontArtist;
   delete fontStd;
   delete fontPl;
   delete lmc;
   delete statusMonitor;

   delete osd;

   free(resDir);
}

//***************************************************************************
// Thread Steering
//***************************************************************************

void cSqueezeOsd::stop()
{
   loopActive = no;
   Cancel(3);             // wait up to 3 seconds for thread was stopping
}

void cSqueezeOsd::view()
{
   visible = yes;
   forceNextDraw = yes;
}

void cSqueezeOsd::hide()
{
   forceNextDraw = yes;
}

//***************************************************************************
// Process Key
//***************************************************************************

int cSqueezeOsd::ProcessKey(int key)
{
   int status;

   if (menu)
   {
      // #TODO add a menuMutex

      status = menu->getActive()->ProcessKey(key);

      if (status == done)
      {
         forceMenuDraw = yes;
         return done;
      }

      if (status == end)
      {
         tell(0, "exit menu level");
         cMenuBase* m = cMenuBase::getActive();

         if (m) delete m;

         if (!cMenuBase::getActive())
         {
            tell(0, "menu closed");
            menu = 0;
         }

         forceMenuDraw = yes;
         return done;
      }
   }

   switch (key)
   {
      case kUp:
      {
         if (plCurrent > 0)
            plCurrent--;

         plUserAction = yes;
         lastScrollAt = time(0);
         forceNextDraw = yes;

         return done;
      }

      case kDown:  
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent++;

         plUserAction = yes;
         lastScrollAt = time(0);
         forceNextDraw = yes;

         return done;
      }

      case kRight:
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent = min(plCurrent+plItems, lmc->getTrackCount()-1);

         plUserAction = yes;
         lastScrollAt = time(0);
         forceNextDraw = yes;

         return done;
      }

      case kLeft:
      {
         if (plCurrent > 0)
            plCurrent = max(plCurrent-plItems, 0);

         plUserAction = yes;
         lastScrollAt = time(0);
         forceNextDraw = yes;

         return done;
      }

      case kOk: 
      {
         if (plCurrent > na && plCurrent < lmc->getTrackCount())
         {
            plUserAction = no;
            lmc->track(plCurrent);
         }

         return done;
      }
   }

   return ignore;
}

//***************************************************************************
// Loop
//***************************************************************************

void cSqueezeOsd::Action()
{
   int changesPending = yes;
   int fullDraw;
   uint64_t lastDraw = 0;

   loopActive = yes;
   currentState = lmc->getPlayerState();   
   visible = yes;

   lmc->update();
   lmc->startNotify();

   while (loopActive && Running())
   {
      if (!lmc->isOpen())
      {
         if (lmc->open(cfg.lmcHost, cfg.lmcPort) != success)
         {
            tell(eloAlways, "Opening connection to LMC server at '%s:%d' failed, retrying in 5 seconds",
                 cfg.lmcHost, cfg.lmcPort);

            sleep(5);
         }
      }

      // scroll

      if (time(0) > lastScrollAt + 10 && plUserAction)
      {
         plUserAction = no;
         forceNextDraw = yes;
      }

      // check for notification with 100ms timeout

      changesPending = lmc->checkNotify(50) == success;

      tell(eloDebug, "looping ... (%d) (%d)", changesPending, forceNextDraw);

      if (!visible)
         continue;

      fullDraw = forceNextDraw || changesPending;

      if (osd && (cTimeMs::Now() > lastDraw+1000 || fullDraw || forceMenuDraw))
      {
         if (fullDraw)
            drawOsd();
         else if (forceMenuDraw)
         {
            drawMenu();
            drawButtons();
         }
         else
         {
            drawProgress();
            drawStatus();
         }

         forceNextDraw = no;
         lastDraw = cTimeMs::Now();

         osd->Flush();  
      }
   }

   lmc->stopNotify();
}

//***************************************************************************
// Initialize
//***************************************************************************

int cSqueezeOsd::init()
{
   if (!osd)
   {
      int width = (cOsd::OsdWidth() - 3 * border) / 2;
      int leftX = width + 2*border;

      // create osd

      osd = cOsdProvider::NewOsd(0, 0, 3);
      tArea control = { 0, 0, cOsd::OsdWidth(), cOsd::OsdHeight(), 32 };

      if (osd->CanHandleAreas(&control, 1) != oeOk)
      {
         tell(eloAlways, "Can't open osd, CanHandleAreas()");
         return fail;
      }

      osd->SetAreas(&control, 1);

      // create fonts

      const cFont* vdrFont = cFont::GetFont(fontOsd);

      fontStd = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 9.0);
      fontTilte = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 13.0);
      fontArtist = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 11.0);
      fontPl = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 7.0);

      // draw black background

      osd->DrawRectangle(0, 0, cOsd::OsdWidth(), cOsd::OsdHeight(), clrBlack);

      // create boxed

      int stHeight = fontStd->Height() + 2 * border;
      int ifoHeight = 2 * fontTilte->Height() + fontArtist->Height() + 3 * fontStd->Height() + 2 * border;
      int plY = border + ifoHeight + 2 * border;
      int plHeight = cOsd::OsdHeight() - plY - stHeight - 2 * border;
      int stY = plY + plHeight + 2 * border;
      int btnWidth = width / 4 - border;
      int btnX = border;
      int menuY = stHeight + 2 * border;

      symbolBoxHeight = btnWidth / 2;
      coverAreaWidth = cOsd::OsdWidth() / 2 - 2 * border;
      coverAreaHeight = cOsd::OsdHeight() - symbolBoxHeight - stHeight - 4 * border;
      int menuHeight = coverAreaHeight - stHeight - border;

      plItems = (plHeight-2*border) / (fontPl->Height()*2 + plItemSpace);
      plItemSpace = ((plHeight-2*border) - (plItems * fontPl->Height()*2)) / plItems;
      plItemHeight = fontPl->Height()*2 + plItemSpace;

      menuItemHeight = fontStd->Height() + menuItemSpace;        // first aproach
      menuItems = (menuHeight-2*border) / menuItemHeight;
      menuItemSpace = ((menuHeight-2*border) - menuItems * menuItemHeight) / menuItems;
      menuItemHeight = fontStd->Height() + menuItemSpace;        // recalc
      visibleMenuItems = (menuHeight-2*border) / menuItemHeight;

      tell(eloDebug, "calculated %d items with a space of %d, hight is %d", 
           plItems, plItemSpace, (plHeight-2*border));

      createBox(pixmapInfo, leftX, border, width, ifoHeight, clrBox, clrBoxBlend, 15);
      createBox(pixmapPlaylist, leftX, plY, width, plHeight, clrBox, clrBoxBlend, 15);
      createBox(pixmapPlCurrent, leftX, plY, width, plItemHeight, clrBox, clrWhite, 15);
      createBox(pixmapStatus, leftX, stY, width, stHeight, clrBox, clrBoxBlend, 15);

      createBox(pixmapMenuTitle, border, border, coverAreaWidth, stHeight, clrBox, clrBoxBlend, 15);
      createBox(pixmapMenu, border, menuY, coverAreaWidth, menuHeight, clrBox, clrBoxBlend, 15);
      createBox(pixmapMenuCurrent, border, menuY, coverAreaWidth, menuItemHeight, clrBox, clrWhite, 15);

      createBox(pixmapBtnRed, btnX, stY, btnWidth, stHeight, 0xFF990000, 0xFFFF0000, 10);
      btnX += btnWidth + border;
      createBox(pixmapBtnGreen, btnX, stY, btnWidth, stHeight, 0xFF009900, 0xFF00EE00, 10);
      btnX += btnWidth + border;
      createBox(pixmapBtnYellow, btnX, stY, btnWidth, stHeight, 0xFF999900, 0xFFEEEE00, 10);
      btnX += btnWidth + border;
      createBox(pixmapBtnBlue, btnX, stY, btnWidth, stHeight, clrBox, clrBoxBlend, 10);
   }

   return done;
}

//***************************************************************************
// Create Box
//***************************************************************************

int cSqueezeOsd::createBox(cPixmap* pixmap[], int x, int y, int width, int height, 
                           tColor color, tColor blend, int radius)
{
   cPixmap::Lock();

   // background pixmap

   pixmap[pmBack] = osd->CreatePixmap(1, cRect(x, y, width, height));
   pixmap[pmBack]->Fill(color);
   DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, true);
   DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, false);
   DrawRoundedCorners(pixmap[pmBack], radius, 0, 0, width, height);

   // front/text pixmap

   pixmap[pmText] = osd->CreatePixmap(2, cRect(x+2*border, y+border, width-4*border, height-2*border));
   pixmap[pmText]->Fill(clrTransparent);

   cPixmap::Unlock();

   return success;
}

//***************************************************************************
// Draw Osd
//***************************************************************************

int cSqueezeOsd::drawOsd()
{
   tell(eloDetail, "Draw OSD");

   // set alpha to force redraw of background boxes

   pixmapInfo[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlaylist[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlCurrent[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapStatus[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnRed[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnGreen[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnYellow[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnBlue[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenuTitle[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenu[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenuCurrent[pmBack]->SetAlpha(ALPHA_TRANSPARENT);

   // draw ...

   if (menu)
      drawMenu();
   else
   {
      pixmapMenuTitle[pmText]->SetAlpha(ALPHA_TRANSPARENT);
      pixmapMenu[pmText]->SetAlpha(ALPHA_TRANSPARENT);

      drawCover();
   }

   drawInfoBox();
   drawPlaylist();
   drawStatus();
   drawButtons();

   return success;
}

//***************************************************************************
// Draw Menu
//***************************************************************************

int cSqueezeOsd::drawMenu()
{
   forceMenuDraw = no;

   pixmapMenu[pmText]->Fill(clrTransparent);         // clear box
   pixmapMenuTitle[pmText]->Fill(clrTransparent);    // clear box

   // title

   pixmapMenuTitle[pmText]->DrawText(cPoint(0, 0), menu->getActive()->Title(),
                                     clrWhite, clrTransparent, fontStd);

   // visible menu items

   int x = 0;
   int y = 0;
   int current = menu->getActive()->getCurrent();
   int count = menu->getActive()->getCount();

   // adjust first Visible;

   if (current < menuTop)
      menuTop = current;
   else if (current >= menuTop + visibleMenuItems)
      menuTop = current - visibleMenuItems +1;

   for (int i = menuTop, cnt = 0; i < count && cnt < visibleMenuItems; i++, cnt++)
   {
      if (i == current)
      {
         int ay = pixmapMenu[pmBack]->ViewPort().Y() + y + border;

         pixmapMenuCurrent[pmBack]->SetViewPort(cRect(pixmapMenuCurrent[pmBack]->ViewPort().X(), ay, 
                                                      pixmapMenuCurrent[pmBack]->ViewPort().Width(), 
                                                      pixmapMenuCurrent[pmBack]->ViewPort().Height()));
      }

      pixmapMenu[pmText]->DrawText(cPoint(x, y),
                                   cString::sprintf("%s", menu->getActive()->getItemTextAt(i)),
                                   clrWhite, clrTransparent, fontStd, pixmapMenu[pmText]->ViewPort().Width());

      y += menuItemHeight;
   }
   
   // 

   pixmapMenuTitle[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapMenuTitle[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapMenu[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapMenu[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapMenuCurrent[pmBack]->SetAlpha(ALPHA_OPAQUE);

   return done;
}

//***************************************************************************
// Draw Info Box
//***************************************************************************

int cSqueezeOsd::drawInfoBox()
{
   LmcCom::TrackInfo* currentTrack = lmc->getCurrentTrack();

   // fill info box

   int x = 0;
   int y = 0;

   cPixmap::Lock();

   pixmapInfo[pmText]->Fill(clrTransparent);     // clear box

   cTextWrapper tw(currentTrack->title, fontTilte, pixmapInfo[pmText]->ViewPort().Width());
   
   for (int l = 0; l < 2; l++)
   {
      if (l < tw.Lines())
         pixmapInfo[pmText]->DrawText(cPoint(x, y), tw.GetLine(l), clrWhite, clrTransparent, fontTilte, pixmapInfo[pmText]->ViewPort().Width());

      y += fontTilte->Height();
   }

   pixmapInfo[pmText]->DrawText(cPoint(x, y), currentTrack->artist, clrWhite, clrTransparent, fontArtist, pixmapInfo[pmText]->ViewPort().Width());
   y += fontArtist->Height();
   pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("Genre: %s   Album: %s", currentTrack->genre, currentTrack->album), clrWhite, 
                                clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
   y += fontStd->Height();
   pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("(%d / %d)", currentState->plIndex+1, currentState->plCount), 
                                clrTextDark, clrTransparent, fontPl, pixmapInfo[pmText]->ViewPort().Width(), 0, taCenter | taTop);
   y += fontPl->Height();
   drawProgress(y);

   pixmapInfo[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapInfo[pmText]->SetAlpha(ALPHA_OPAQUE);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Progress
//***************************************************************************

int cSqueezeOsd::drawProgress(int y)
{
   static int yLast = 0;
   static int lastTime = 0;

   int time;
   LmcCom::TrackInfo* currentTrack = lmc->getCurrentTrack();

   if (y != na)  yLast = y;

   if (strcmp(currentState->mode, "play") != 0 || !currentTrack || !currentTrack->duration)
      time = lastTime;
   else
      lastTime = time = currentState->trackTime + (cTimeMs::Now() - currentState->updatedAt) / 1000;

   const cRect rect = pixmapInfo[pmText]->ViewPort();

   int total = currentTrack->duration;
   int remaining = total - time;

   cString begin = cString::sprintf("%d:%02d", time/tmeSecondsPerMinute, time%tmeSecondsPerMinute);
   cString end = cString::sprintf("-%d:%02d", remaining/tmeSecondsPerMinute, remaining%tmeSecondsPerMinute);

   int barX = fontStd->Width(begin) + 10;
   int barWidth = rect.Width() - barX - fontStd->Width(end) - 20;

   double percent = time / (total / 100.0);
   int off = barWidth - ((barWidth / 100.0) * percent);
   int barHeight = fontStd->Height();

   // clear area

   pixmapInfo[pmText]->DrawRectangle(cRect(0, yLast, rect.Width(), barHeight), clrTransparent);
      
   // text

   pixmapInfo[pmText]->DrawText(cPoint(0, yLast), begin, clrWhite, clrTransparent, fontStd);
   pixmapInfo[pmText]->DrawText(cPoint(barX+barWidth+10, yLast), end, clrWhite, clrTransparent, fontStd);

   // progess bar

   pixmapInfo[pmText]->DrawRectangle(cRect(barX,   yLast,   barWidth, barHeight), clrWhite);
   pixmapInfo[pmText]->DrawRectangle(cRect(barX+1, yLast+1, barWidth-2, barHeight-2), 0xFF303060);
   pixmapInfo[pmText]->DrawRectangle(cRect(barX+3, yLast+3, barWidth-off-6, barHeight-6), clrWhite);

   return done;
}

//***************************************************************************
// Draw Playlist
//***************************************************************************

int cSqueezeOsd::drawPlaylist()
{
   static int lastCount = na;
   int y = 0;

   cImage* imgSpeaker = 0;
   int imgWH = pixmapPlCurrent[pmText]->ViewPort().Height() / 3.0 * 2.0;
   int coverHeight = pixmapPlCurrent[pmText]->ViewPort().Height();
   int imgX = pixmapPlCurrent[pmText]->ViewPort().Width() - imgWH;
   int imgY = imgWH / 4;

   cPixmap::Lock();

   pixmapPlaylist[pmText]->Fill(clrTransparent);       // clear box

   // set focus to current if no user interactivity or if count changed

   if (!plUserAction || lmc->getTrackCount() != lastCount)
   {
      plCurrent = currentState->plIndex;
      plTop = max(0, currentState->plIndex-2);
   }

   lastCount = lmc->getTrackCount();

   if (plCurrent < plTop)
      plTop = plCurrent;
   else if (plCurrent >= plTop + plItems)
      plTop = plCurrent - plItems +1;

   for (int i = plTop, cnt = 0; i < lmc->getTrackCount() && cnt < plItems; i++, cnt++)
   {
      tColor color = clrTextDark;

      if (i == plCurrent)
      {
         int ay = pixmapPlaylist[pmBack]->ViewPort().Y() + y;

         pixmapPlCurrent[pmBack]->SetViewPort(cRect(pixmapPlCurrent[pmBack]->ViewPort().X(), ay, 
                                                    pixmapPlCurrent[pmBack]->ViewPort().Width(), 
                                                    pixmapPlCurrent[pmBack]->ViewPort().Height()));
      }

      if (i == currentState->plIndex)
      {
         color = clrWhite;
         drawSymbol("speaker.png", imgX, imgY+y, imgWH, imgWH, pixmapPlaylist[pmText]);
      }

      drawTrackCover(pixmapPlaylist[pmText], lmc->getTrack(i)->id, 0, y, coverHeight);

      int x = coverHeight + border;
      pixmapPlaylist[pmText]->DrawText(cPoint(x, y),
                                       cString::sprintf("%s", lmc->getTrack(i)->title),
                                       color, clrTransparent, fontPl, pixmapPlaylist[pmText]->ViewPort().Width());

      y += fontPl->Height();

      pixmapPlaylist[pmText]->DrawText(cPoint(x, y),
                               cString::sprintf("%s", lmc->getTrack(i)->artist), 
                               color, clrTransparent, fontPl, pixmapPlaylist[pmText]->ViewPort().Width());

      y += fontPl->Height()+plItemSpace;
   }

   pixmapPlaylist[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapPlaylist[pmText]->SetAlpha(ALPHA_OPAQUE);

   if (!menu)
      pixmapPlCurrent[pmBack]->SetAlpha(ALPHA_OPAQUE);

   cPixmap::Unlock();

   delete imgSpeaker;

   return done;
}

//***************************************************************************
// Draw Info Box
//***************************************************************************

int cSqueezeOsd::drawStatus()
{
   char* name = 0;

   // fill status box

   cPixmap::Lock();

   pixmapStatus[pmText]->Fill(clrTransparent);       // clear box

   cString date = DayDateTime();

   int x = pixmapStatus[pmText]->ViewPort().Width() - fontStd->Width(date);
   int y = (pixmapStatus[pmText]->ViewPort().Height()-fontStd->Height()) / 2;

   pixmapStatus[pmText]->DrawText(cPoint(x, y), date, 
                                  clrWhite, clrTransparent, fontStd, 
                                  pixmapStatus[pmText]->ViewPort().Width());

   pixmapStatus[pmText]->DrawText(cPoint(0, y), currentState->plName, 
                                  clrWhite, clrTransparent, fontStd, 
                                  pixmapStatus[pmText]->ViewPort().Width());


   pixmapStatus[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapStatus[pmText]->SetAlpha(ALPHA_OPAQUE);

   cPixmap::Unlock();

   // draw status icons above the color buttons ..

   y = pixmapBtnRed[pmBack]->ViewPort().Y() - 2*border - symbolBoxHeight;
   x = 3 * border;

   asprintf(&name, "%s.png", currentState->mode);
   drawSymbol(name, x, y, symbolBoxHeight, symbolBoxHeight);
   free(name);
   
   x += symbolBoxHeight + 2 * border;
   asprintf(&name, "shuffle%d.png", currentState->plShuffle);
   drawSymbol(name, x, y, symbolBoxHeight, symbolBoxHeight);
   free(name);

   x += symbolBoxHeight + 2 * border;
   asprintf(&name, "repeat%d.png", currentState->plRepeat);
   drawSymbol(name, x, y, symbolBoxHeight, symbolBoxHeight);
   free(name);

   x += symbolBoxHeight + 4 * border;
   drawVolume(x, y, ((cOsd::OsdWidth() - 3 * border) / 2)  - x - 4*border);

   return done;
}

//***************************************************************************
// Draw Volume
//***************************************************************************

int cSqueezeOsd::drawVolume(int x, int y, int width)
{
   double percent = currentState->volume / (100 / 100.0);
   int off = width - ((width / 100.0) * percent);
   int barHeight = fontPl->Height();

   y = y + (symbolBoxHeight - barHeight) / 2;

   osd->DrawText(x, y-fontPl->Height(), tr("Volume"), clrWhite, clrBlack, fontPl, width, 0, taTop|taCenter);

   // progess bar

   osd->DrawRectangle(x,   y,   x+width, y+barHeight, clrWhite);
   osd->DrawRectangle(x+1, y+1, x+1+width-2, y+1+barHeight-2, 0xFF303060);
   osd->DrawRectangle(x+3, y+3, x+3+width-off-6, y+3+barHeight-6, clrWhite);

   return done;
}

//***************************************************************************
// Draw Buttons
//***************************************************************************

int cSqueezeOsd::drawButtons()
{
   // fill status box

   cPixmap::Lock();

   pixmapBtnRed[pmText]->Fill(clrTransparent);
   pixmapBtnGreen[pmText]->Fill(clrTransparent);
   pixmapBtnYellow[pmText]->Fill(clrTransparent);
   pixmapBtnBlue[pmText]->Fill(clrTransparent);  

   pixmapBtnRed[pmText]->DrawText(cPoint(0, 0), Red(),
                                  clrWhite, clrTransparent, fontStd, 
                                  pixmapBtnRed[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   pixmapBtnGreen[pmText]->DrawText(cPoint(0, 0), Green(), 
                                    clrWhite, clrTransparent, fontStd, 
                                    pixmapBtnGreen[pmText]->ViewPort().Width(), 0, taCenter | taTop);
   
   pixmapBtnYellow[pmText]->DrawText(cPoint(0, 0), Yellow(),
                                     clrWhite, clrTransparent, fontStd, 
                                     pixmapBtnYellow[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   pixmapBtnBlue[pmText]->DrawText(cPoint(0, 0), Blue(), 
                                   clrWhite, clrTransparent, fontStd, 
                                   pixmapBtnBlue[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   pixmapBtnRed[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnRed[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnGreen[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnGreen[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnYellow[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnYellow[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnBlue[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapBtnBlue[pmText]->SetAlpha(ALPHA_OPAQUE);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Cover
//***************************************************************************

int cSqueezeOsd::drawTrackCover(cPixmap* pixmap, int tid, int x, int y, int size)
{
   MemoryStruct cover;

   if (!osd)
      return done;

   lmc->getCover(&cover, tid);
   storeFile(&cover, "/tmp/squeeze_track_cover.jpg");

   if (imgLoader->loadImage("/tmp/squeeze_track_cover.jpg") == success)
   {
      cImage* image = imgLoader->createImage(size, size, yes);

      if (image)
      {
         pixmap->DrawImage(cPoint(x, y), *image);
         delete image;
      }
   }
      
   return done;
}

//***************************************************************************
// Draw Cover
//***************************************************************************

int cSqueezeOsd::drawCover()
{
   MemoryStruct cover;

   if (!osd)
      return done;

   lmc->getCurrentCover(&cover);
   storeFile(&cover, "/tmp/squeeze_cover.jpg");

   if (imgLoader->loadImage("/tmp/squeeze_cover.jpg") == success)
   {
      unsigned short border = 140;
      unsigned short imgWidthHeight = coverAreaWidth - 2 * border;

      cImage* image = imgLoader->createImage(imgWidthHeight, imgWidthHeight, yes);

      if (image)
      {
         osd->DrawRectangle(0, 0, coverAreaWidth, coverAreaHeight, clrBlack);
         osd->DrawImage(cPoint(border, (coverAreaHeight-imgWidthHeight) / 2), *image);
         delete image;
      }
   }

   return success;
}

//***************************************************************************
// Draw Symbol
//***************************************************************************

int cSqueezeOsd::drawSymbol(const char* name, int x, int y, int width, int height,
   cPixmap* pixmap)
{
   char* path = 0;
   cImage* image = 0;

   asprintf(&path, "%s/squeezebox/%s", resDir, name);
   image = imgLoader->createImageFromFile(path, width, height, yes);

   if (image)
   {
      if (pixmap)
         pixmap->DrawImage(cPoint(x, y), *image);
      else
         osd->DrawImage(cPoint(x, y), *image);
   }

   free(path);
   delete image;

   return done;
}
