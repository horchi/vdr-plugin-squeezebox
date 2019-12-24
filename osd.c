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
   forceNextDraw = yes;
   forceMenuDraw = no;
   forcePlaylistDraw = yes;

   alpha = ALPHA_OPAQUE;
   lastActivityAt = time(0);

   border = 10;
   plTop = 0;
   plCurrent = 0;              // current user selected item, or current track (if no user action)
   plUserAction = no;          // indicates that user scrolling the list
   lastScrollAt = time(0);
   plItemSpace = 10;
   plItems = 0;
   plItemHeight = 0;
   resDir = strdup(aResDir);
   buttonLevel = 0;
   menu = 0;
   menuTop = 0;
   visibleMenuItems = 0;
   menuItemHeight = 0;
   menuItemSpace = 5;

   lyricsScrollPos = 0;
   lyricsDrawportHeight = 0;
   lyricsScrollingAt = time(0);
   nextScrollStep = cTimeMs::Now();
   *lastLyrics = 0;

   statusMonitor = new cMyStatus(this);

   osd = 0;

   pixmapLyrics = 0;
   memset(pixmapCover, 0, sizeof(pixmapCover));
   memset(pixmapInfo, 0, sizeof(pixmapInfo));
   memset(pixmapPlaylist, 0, sizeof(pixmapPlaylist));
   memset(pixmapPlCurrent, 0, sizeof(pixmapPlCurrent));
   memset(pixmapStatus, 0, sizeof(pixmapStatus));
   memset(pixmapSymbols, 0, sizeof(pixmapSymbols));
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
   fontLyrics = 0;

   clrBox = 0xFF000040;
   clrBoxBlend = 0xFF0000AA;
   clrTextDark = 0xFF808080;

   osd2web = 0;

   lmc = new LmcCom(cfg.mac);
   imgLoader = new cImageMagickWrapper();

   if (lmc->open(cfg.lmcHost, cfg.lmcPort) != success)
   {
      tell(eloAlways, "Opening connection to LMC server at '%s:%d' failed",
           cfg.lmcHost, cfg.lmcPort);
   }
}

cSqueezeOsd::~cSqueezeOsd()
{
   stop();
   exit();

   delete statusMonitor;
   delete lmc;
   delete imgLoader;
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
   forceNextDraw = yes;
}

void cSqueezeOsd::hide()
{
   forceNextDraw = yes;
}

//***************************************************************************
// Initialize
//***************************************************************************

int cSqueezeOsd::init()
{
   int res = success;

   if (!osd)
   {
      int width = (cOsd::OsdWidth() - 3 * border) / 2;
      int leftX = width + 2*border;

      // create osd

      osd = cOsdProvider::NewOsd(0, 0, 1);
      tArea control = { 0, 0, cOsd::OsdWidth(), cOsd::OsdHeight(), 32 };

      if (osd->CanHandleAreas(&control, 1) != oeOk)
      {
         tell(eloAlways, "Can't open osd, CanHandleAreas() failed");
         return fail;
      }

      osd->SetAreas(&control, 1);

      // create fonts

      const cFont* vdrFont = cFont::GetFont(fontOsd);

      fontStd = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 9.0);
      fontTilte = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 13.0);
      fontArtist = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 11.0);
      fontPl = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 7.0);
      fontLyrics = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 6.0);

      // draw black background

      osd->DrawRectangle(0, 0, cOsd::OsdWidth(), cOsd::OsdHeight(), clrBlack);

      // create boxed

      int stHeight = fontStd->Height() + 2 * border;
      int ifoHeight = 2 * fontTilte->Height() + fontArtist->Height() + 4 * fontStd->Height() + 2 * border;
      int plY = border + ifoHeight + 2 * border;
      int plHeight = cOsd::OsdHeight() - plY - stHeight - 2 * border;
      int stY = plY + plHeight + 2 * border;
      int btnWidth = width / 4 - border;
      int btnX = border;
      int menuY = stHeight + 2 * border;

      int symbolBoxHeight = btnWidth / 2;
      int coverAreaWidth = cOsd::OsdWidth() / 2 - 2 * border;
      int coverAreaHeight = cOsd::OsdHeight() - symbolBoxHeight - stHeight - 5 * border;
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

      res += createBox(pixmapCover, border, border, coverAreaWidth, coverAreaHeight, clrBlack, clrWhite, 15);
      res += createBox(pixmapInfo, leftX, border, width, ifoHeight, clrBox, clrBoxBlend, 15);
      res += createBox(pixmapPlaylist, leftX, plY, width, plHeight, clrBox, clrBoxBlend, 15);
      res += createBox(pixmapPlCurrent, leftX, plY, width, plItemHeight, clrBox, clrWhite, 15);
      res += createBox(pixmapStatus, leftX, stY, width, stHeight, clrBox, clrBoxBlend, 15);

      res += createBox(pixmapMenuTitle, border, border, coverAreaWidth, stHeight, clrBox, clrBoxBlend, 15);
      res += createBox(pixmapMenu, border, menuY, coverAreaWidth, menuHeight, clrBox, clrBoxBlend, 15);
      res += createBox(pixmapMenuCurrent, border, menuY, coverAreaWidth, menuItemHeight, clrBox, clrWhite, 15);

      res += createBox(pixmapBtnRed, btnX, stY, btnWidth, stHeight, 0xFF990000, 0xFFFF0000, 10);
      btnX += btnWidth + border;
      res += createBox(pixmapBtnGreen, btnX, stY, btnWidth, stHeight, 0xFF009900, 0xFF00EE00, 10);
      btnX += btnWidth + border;
      res += createBox(pixmapBtnYellow, btnX, stY, btnWidth, stHeight, 0xFF999900, 0xFFEEEE00, 10);
      btnX += btnWidth + border;
      res += createBox(pixmapBtnBlue, btnX, stY, btnWidth, stHeight, clrBox, clrBoxBlend, 10);

      if (!res)
         res += createBox(pixmapSymbols, border, pixmapBtnRed[pmBack]->ViewPort().Y() - 2*border - symbolBoxHeight,
                          coverAreaWidth, symbolBoxHeight, clrBox, clrBoxBlend, 15);
   }

   if (res != success && osd)
   {
      delete osd;
      osd = 0;
   }

   return res;
}

int cSqueezeOsd::exit()
{
   if (osd)
   {
      osd->DestroyPixmap(pixmapCover[0]);
      osd->DestroyPixmap(pixmapCover[1]);
      osd->DestroyPixmap(pixmapInfo[0]);
      osd->DestroyPixmap(pixmapInfo[1]);
      osd->DestroyPixmap(pixmapPlaylist[0]);
      osd->DestroyPixmap(pixmapPlaylist[1]);
      osd->DestroyPixmap(pixmapPlCurrent[0]);
      osd->DestroyPixmap(pixmapPlCurrent[1]);
      osd->DestroyPixmap(pixmapStatus[0]);
      osd->DestroyPixmap(pixmapStatus[1]);
      osd->DestroyPixmap(pixmapSymbols[0]);
      osd->DestroyPixmap(pixmapSymbols[1]);
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

      if (pixmapLyrics) osd->DestroyPixmap(pixmapLyrics);
   }

   delete fontTilte;  fontTilte = 0;
   delete fontArtist; fontArtist = 0;
   delete fontStd;    fontStd = 0;
   delete fontPl;     fontPl = 0;
   delete fontLyrics; fontLyrics = 0;

   return done;
}

//***************************************************************************
// Create Box
//***************************************************************************

int cSqueezeOsd::createBox(cPixmap* pixmap[], int x, int y, int width, int height,
                           tColor color, tColor blend, int radius)
{
   if (!osd)
      return fail;

   cPixmap::Lock();

   // background pixmap

   if (!(pixmap[pmBack] = osd->CreatePixmap(1, cRect(x, y, width, height))))
   {
      cPixmap::Unlock();
      tell(eloAlways, "Error: osd->CreatePixmap(%d,%d,%d,%d) failed", x, y, width, height);
      return fail;
   }

   pixmap[pmBack]->Fill(color);

   if (cfg.rounded)
   {
      DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, true);
      DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, false);
      DrawRoundedCorners(pixmap[pmBack], radius, 0, 0, width, height);
   }

   // front/text pixmap

   if (!(pixmap[pmText] = osd->CreatePixmap(2, cRect(x+2*border, y+border, width-4*border, height-2*border))))
      return fail;

   pixmap[pmText]->Fill(clrTransparent);

   cPixmap::Unlock();

   return success;
}

//***************************************************************************
// Process Key
//***************************************************************************

int cSqueezeOsd::ProcessKey(int key)
{
   int status;

   if (menu)
   {
      status = menu->getActive()->ProcessKey(key);

      if (status == done)
      {
         forceMenuDraw = yes;
         return done;
      }

      if (status == end)
      {
         while (cMenuBase* m = cMenuBase::getActive())
            delete m;

         menu = 0;
         forceNextDraw = yes;

         return done;
      }

      if (status == back)
      {
         cMenuBase* m = cMenuBase::getActive();

         if (m) delete m;

         if (!cMenuBase::getActive())
         {
            menu = 0;
            forceNextDraw = yes;
         }
         else
            forceMenuDraw = yes;

         return done;
      }
   }

   switch (key)
   {
      case kUp|k_Repeat:
      case kUp:
      {
         if (plCurrent > 0)
            plCurrent--;

         plUserAction = yes;
         lastScrollAt = time(0);
         forcePlaylistDraw = yes;

         return done;
      }

      case kDown|k_Repeat:
      case kDown:
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent++;

         plUserAction = yes;
         lastScrollAt = time(0);
         forcePlaylistDraw = yes;

         return done;
      }

      case kRight|k_Repeat:
      case kRight:
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent = min(plCurrent+plItems, lmc->getTrackCount()-1);

         plUserAction = yes;
         lastScrollAt = time(0);
         forcePlaylistDraw = yes;

         return done;
      }

      case kLeft|k_Repeat:
      case kLeft:
      {
         if (plCurrent > 0)
            plCurrent = max(plCurrent-plItems, 0);

         plUserAction = yes;
         lastScrollAt = time(0);
         forcePlaylistDraw = yes;

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
   static int count = 0;

   int changesPending = yes;
   int fullDraw;
   uint64_t lastDraw = 0;

   osd2web = cPluginManager::GetPlugin("osd2web");
   loopActive = yes;
   currentState = lmc->getPlayerState();

   lmc->update();
   lmc->startNotify();

   if (strcmp(currentState->mode, "play") != 0)
      lmc->play();

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
         forcePlaylistDraw = yes;
      }

      // check for notification with 50ms timeout

      changesPending = lmc->checkNotify(0) == success;
      tell(eloDebug2, "looping %d ... (%d) (%d)", count++, changesPending, forceNextDraw);

      usleep(10000);   // #TODO use mutex wait condition instead

      // shade on inactivity

      if (cfg.shadeTime > 0)
      {
         unsigned short lastAlpha = alpha;

         if (lastActivityAt + cfg.shadeTime < time(0))
            alpha = (0xff / 100.0) * (100.0 - cfg.shadeLevel);
         else
            alpha = ALPHA_OPAQUE;

         if (alpha != lastAlpha)
            forceNextDraw = yes;
      }

      // scroll lyrics

      if (osd && cTimeMs::Now() >= nextScrollStep)
      {
         TrackInfo* currentTrack = lmc->getCurrentTrack();

         if (!isEmpty(currentTrack->lyrics))
         {
            scrollLyrics();
            osd->Flush();
         }
      }

      // check force

      fullDraw = forceNextDraw || changesPending;

      // draw osd

      if (osd && (cTimeMs::Now() > lastDraw+1000 || fullDraw || forceMenuDraw || forcePlaylistDraw))
      {
         if (fullDraw)
         {
            drawOsd();
         }
         else if (forceMenuDraw && menu)
         {
            drawCover();
            drawMenu();
            drawButtons();
         }
         else if (forcePlaylistDraw && !menu)
         {
            drawPlaylist();
            drawProgress();
         }
         else
         {
            drawProgress();
            drawStatus();
         }

         forceNextDraw = no;
         forcePlaylistDraw = no;
         forceMenuDraw = no;
         lastDraw = cTimeMs::Now();

         osd->Flush();
      }
   }

   lmc->stopNotify();
}

//***************************************************************************
// Draw Osd
//***************************************************************************

int cSqueezeOsd::drawOsd()
{
   tell(eloDebug, "Draw OSD");

   // set alpha to force redraw of background boxes

   pixmapCover[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapInfo[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlaylist[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlCurrent[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapStatus[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapSymbols[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnRed[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnGreen[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnYellow[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnBlue[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenuTitle[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenu[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenuCurrent[pmBack]->SetAlpha(ALPHA_TRANSPARENT);

   // draw ...

   drawCover();

   if (menu)
      drawMenu();

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
   int x = 0;
   int y = 0;
   cMenuBase* active = menu ? menu->getActive() : 0;

   pixmapCover[pmText]->SetAlpha(0x66);

   if (!active)
      return done;

   pixmapMenu[pmText]->Fill(clrTransparent);         // clear box
   pixmapMenuTitle[pmText]->Fill(clrTransparent);    // clear box

   // title

   pixmapMenuTitle[pmText]->DrawText(cPoint(0, 0), active->Title(),
                                     clrWhite, clrTransparent, fontStd);

   // visible menu items

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

   pixmapMenuTitle[pmBack]->SetAlpha(alpha);
   pixmapMenuTitle[pmText]->SetAlpha(alpha);
   pixmapMenu[pmBack]->SetAlpha(alpha);
   pixmapMenu[pmText]->SetAlpha(alpha);
   pixmapMenuCurrent[pmBack]->SetAlpha(alpha);
   pixmapMenuCurrent[pmText]->SetAlpha(alpha);

   return done;
}

//***************************************************************************
// Draw Info Box
//***************************************************************************

int cSqueezeOsd::drawInfoBox()
{
   TrackInfo* currentTrack = lmc->getCurrentTrack();

   sendInfoBox(currentTrack);

   // fill info box

   int x = 0;
   int y = 0;

   if (!osd)
      return fail;

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

   if (currentTrack->remote)
   {
      pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("%s", currentTrack->remoteTitle),
                                   clrWhite, clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
      y += fontStd->Height();
      pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("%dkb/s CBR, %s", currentTrack->bitrate, currentTrack->contentType),
                                   clrWhite, clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
      y += fontStd->Height();
   }
   else
   {
      char tmp[10];

      pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("Genre: %s",
                                                                  currentTrack->genre),
                                   clrWhite, clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
      y += fontStd->Height();

      if (currentTrack->year) sprintf(tmp, "(%d)", currentTrack->year);
      pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("Album: %s %s", currentTrack->album, tmp),
                                   clrWhite, clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
      y += fontStd->Height();
   }

   if (currentTrack->duration)
      pixmapInfo[pmText]->DrawText(cPoint(x, y), cString::sprintf("(%d / %d)", currentState->plIndex+1, currentState->plCount),
                                   clrTextDark, clrTransparent, fontPl, pixmapInfo[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   y += fontPl->Height();
   drawProgress(y);

   pixmapInfo[pmBack]->SetAlpha(alpha);
   pixmapInfo[pmText]->SetAlpha(alpha);

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
   int barHeight = fontStd->Height();
   TrackInfo* currentTrack = lmc->getCurrentTrack();
   const cRect rect = pixmapInfo[pmText]->ViewPort();

   if (y != na)  yLast = y;

   if (strcmp(currentState->mode, "play") != 0 || !currentTrack) //  (!currentTrack->duration)
      time = lastTime;
   else
      lastTime = time = currentState->trackTime + (cTimeMs::Now() - currentState->updatedAt) / 1000;

   // clear area

   pixmapInfo[pmText]->DrawRectangle(cRect(0, yLast, rect.Width(), barHeight), clrTransparent);

   if (currentTrack->duration)
   {
      int total = currentTrack->duration;
      int remaining = total - time;

      cString begin = cString::sprintf("%d:%02d", time/tmeSecondsPerMinute, time%tmeSecondsPerMinute);
      cString end = cString::sprintf("-%d:%02d", remaining/tmeSecondsPerMinute, remaining%tmeSecondsPerMinute);

      int barX = fontStd->Width(begin) + 10;
      int barWidth = rect.Width() - barX - fontStd->Width(end) - 20;

      double percent = time / (total / 100.0);
      int off = barWidth - ((barWidth / 100.0) * percent);

      // progress time / remaining time

      pixmapInfo[pmText]->DrawText(cPoint(0, yLast), begin, clrWhite, clrTransparent, fontStd);
      pixmapInfo[pmText]->DrawText(cPoint(barX+barWidth+10, yLast), end, clrWhite, clrTransparent, fontStd);

      // progess bar

      pixmapInfo[pmText]->DrawRectangle(cRect(barX,   yLast,   barWidth, barHeight), clrWhite);
      pixmapInfo[pmText]->DrawRectangle(cRect(barX+1, yLast+1, barWidth-2, barHeight-2), 0xFF303060);
      pixmapInfo[pmText]->DrawRectangle(cRect(barX+3, yLast+3, barWidth-off-6, barHeight-6), clrWhite);
   }
   else
   {
      // progress time

      cString begin = cString::sprintf("%s: %d:%02d", tr("Duration"), time/tmeSecondsPerMinute, time%tmeSecondsPerMinute);
      pixmapInfo[pmText]->DrawText(cPoint(0, yLast), begin, clrWhite, clrTransparent, fontStd);

      pixmapInfo[pmText]->DrawText(cPoint(0, yLast), cString::sprintf("(%d / %d)", currentState->plIndex+1, currentState->plCount),
                                   clrTextDark, clrTransparent, fontPl, pixmapInfo[pmText]->ViewPort().Width(), 0, taRight | taTop);
   }

   return done;
}

//***************************************************************************
// Draw Playlist
//***************************************************************************

int cSqueezeOsd::drawPlaylist()
{
   static int lastCount = na;

   if (!osd)
      return fail;

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

   int cnt = 0;

   for (int i = plTop; i < lmc->getTrackCount() && cnt < plItems; i++, cnt++)
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
         drawSymbol(pixmapPlaylist[pmText], "speaker.png", imgX, imgY+y, imgWH, imgWH);
      }

      drawTrackCover(pixmapPlaylist[pmText], lmc->getTrack(i), 0, y, coverHeight);

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

   pixmapPlaylist[pmBack]->SetAlpha(alpha);
   pixmapPlaylist[pmText]->SetAlpha(alpha);

   if (!menu)
      pixmapPlCurrent[pmBack]->SetAlpha(alpha);

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

   if (!osd)
      return fail;

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


   pixmapStatus[pmBack]->SetAlpha(alpha);
   pixmapStatus[pmText]->SetAlpha(alpha);

   // symbols
   // draw status icons above the color buttons ..

   pixmapSymbols[pmText]->Fill(clrTransparent);       // clear box

   x = border;

   if (!isEmpty(currentState->mode))
   {
      asprintf(&name, "%s.png", currentState->mode);
      drawSymbol(pixmapSymbols[pmText], name, x, 0);
      free(name);
   }

   x += 2 * border;
   asprintf(&name, "shuffle%d.png", currentState->plShuffle);
   drawSymbol(pixmapSymbols[pmText], name, x, 0);
   free(name);

   x += 2 * border;
   asprintf(&name, "repeat%d.png", currentState->plRepeat);
   drawSymbol(pixmapSymbols[pmText], name, x, 0);
   free(name);

   x += 4 * border;
   drawVolume(pixmapSymbols[pmText], x, 0, pixmapSymbols[pmText]->ViewPort().Width() - x - 4*border);

   pixmapSymbols[pmBack]->SetAlpha(alpha);
   pixmapSymbols[pmText]->SetAlpha(alpha);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Volume
//***************************************************************************

int cSqueezeOsd::drawVolume(cPixmap* pixmap, int x, int y, int width)
{
   if (!osd)
      return fail;

   double percent = currentState->volume / (100 / 100.0);
   int off = width - ((width / 100.0) * percent);
   int barHeight = fontPl->Height();

   cPixmap::Lock();

   y = y + (pixmapSymbols[pmText]->ViewPort().Height() - 2*barHeight) / 2;

   pixmap->DrawText(cPoint(x, y), tr("Volume"), clrWhite, clrTransparent,
                    fontPl, width, 0, taTop|taCenter);

   y += fontPl->Height();

   // progess bar

   pixmap->DrawRectangle(cRect(x,   y,   width, barHeight), clrWhite);
   pixmap->DrawRectangle(cRect(x+1, y+1, width-2, barHeight-2), 0xFF303060);
   pixmap->DrawRectangle(cRect(x+3, y+3, width-off-6, barHeight-6), clrWhite);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Buttons
//***************************************************************************

int cSqueezeOsd::drawButtons()
{
   if (!osd)
      return fail;

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

   pixmapBtnRed[pmBack]->SetAlpha(alpha);
   pixmapBtnRed[pmText]->SetAlpha(alpha);
   pixmapBtnGreen[pmBack]->SetAlpha(alpha);
   pixmapBtnGreen[pmText]->SetAlpha(alpha);
   pixmapBtnYellow[pmBack]->SetAlpha(alpha);
   pixmapBtnYellow[pmText]->SetAlpha(alpha);
   pixmapBtnBlue[pmBack]->SetAlpha(alpha);
   pixmapBtnBlue[pmText]->SetAlpha(alpha);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Cover
//***************************************************************************

int cSqueezeOsd::drawTrackCover(cPixmap* pixmap, TrackInfo* track,
                                int x, int y, int size)
{
   cImage* image = 0;
   MemoryStruct cover;
   std::string hash;

   if (!osd)
      return fail;

   cPixmap::Lock();

   if (!isEmpty(track->artworkurl))
      hash = track->artworkurl;
   else if (!isEmpty(track->artworkTrackId))
      hash = track->artworkTrackId;
   else
      hash = num2Str(track->id).c_str();

   // clear cache on metadata change

   if (lmc->hasMetadataChanged())
      imgLoader->clearCache();

   // check cache

   image = imgLoader->fromCache(hash);

   if (!image)
   {
      if (lmc->getCover(&cover, track) == success)
      {
         if (imgLoader->loadImage(cover.memory, cover.size) == success)
         {
            image = imgLoader->createImage(size, size, yes);
            imgLoader->addCache(hash, image);
         }
      }
   }

   if (image)
      pixmap->DrawImage(cPoint(x, y), *image);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Cover
//***************************************************************************

int cSqueezeOsd::drawCover()
{
   MemoryStruct cover;
   TrackInfo* currentTrack = lmc->getCurrentTrack();
   std::string hash;
   cImage* image = 0;

   int y = 0; // pixmapCover[pmText]->ViewPort().Height() / 4.0;
   int imgHW = pixmapCover[pmText]->ViewPort().Height() / 4.0 * 4.0;

   if (!osd)
      return fail;

   if (!isEmpty(currentTrack->lyrics) && !menu)
   {
      y = 10;
      imgHW = pixmapCover[pmText]->ViewPort().Height() / 2;
   }

   if (!isEmpty(currentTrack->artworkurl))
      hash = std::string("cover_") + currentTrack->artworkurl;
   else if (!isEmpty(currentTrack->artworkTrackId))
      hash = std::string("cover_") + currentTrack->artworkTrackId;
   else
      hash = std::string("cover_") + num2Str(currentTrack->id).c_str();

   cPixmap::Lock();

   pixmapMenuTitle[pmText]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapMenu[pmText]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapCover[pmText]->Fill(clrTransparent);        // clear box

   cPixmap::Unlock();

   // cleanup image cache

   if (lmc->hasMetadataChanged())
      imgLoader->clearCache();

   // check cache

   image = imgLoader->fromCache(hash);

   if (!image)
   {
      if (lmc->getCurrentCover(&cover, currentTrack) == success)
      {
         if (imgLoader->loadImage(cover.memory, cover.size) == success)
         {
            // optional store the cover on FS
            // storeFile(&cover, "/tmp/squeeze_cover.jpg");

            image = imgLoader->createImage(imgHW, imgHW, yes);
            imgLoader->addCache(hash, image);
         }
      }
   }

   cPixmap::Lock();

   if (image)
   {
      int x = (pixmapCover[pmText]->ViewPort().Width() - imgHW) / 2;

      pixmapCover[pmText]->DrawImage(cPoint(x, y), *image);
      y += imgHW;
   }

   // ------------------------------
   // lyrics

   if (pixmapLyrics && (menu || isEmpty(currentTrack->lyrics)))
   {
      pixmapLyrics->SetAlpha(ALPHA_TRANSPARENT);
   }

   else if (!isEmpty(currentTrack->lyrics) && !menu)
   {
      cTextWrapper tw(currentTrack->lyrics, fontLyrics, pixmapCover[pmText]->ViewPort().Width());

      int height = pixmapCover[pmText]->ViewPort().Height() - y;
      int width = pixmapCover[pmText]->ViewPort().Width();
      int x = pixmapCover[pmText]->ViewPort().X();

      lyricsDrawportHeight = fontLyrics->Height() * tw.Lines();

      if (strncmp(lastLyrics, tw.GetLine(0), 200) != 0)
      {
         lyricsScrollPos = 0;
         snprintf(lastLyrics, 200,  "%s", tw.GetLine(0));

         if (pixmapLyrics) { osd->DestroyPixmap(pixmapLyrics); pixmapLyrics = 0; }

         if (!pixmapLyrics)
         {
            pixmapLyrics = osd->CreatePixmap(1, cRect(x, y, width, height), cRect(0, 0, width, lyricsDrawportHeight));
            pixmapLyrics->Fill(clrTransparent);
         }

         int yl = 0;

         for (int l = 0; l < tw.Lines(); l++)
         {
            pixmapLyrics->DrawText(cPoint(0, yl), tw.GetLine(l),
                                   clrWhite, clrTransparent, fontLyrics,
                                   pixmapLyrics->ViewPort().Width());

            yl += fontLyrics->Height();
         }

         nextScrollStep = cTimeMs::Now() + 50;

         // scrollLyrics();
      }
   }

   pixmapCover[pmBack]->SetAlpha(alpha);
   pixmapCover[pmText]->SetAlpha(alpha);

   cPixmap::Unlock();

   return success;
}

//***************************************************************************
// Scroll Lyrics
//***************************************************************************

int cSqueezeOsd::scrollLyrics()
{
   if (!osd)
      return fail;

   if (menu || !pixmapLyrics || lyricsScrollingAt >= time(0))
      return done;

   if (lyricsScrollPos == 0)
      lyricsScrollingAt = time(0) + 5;

   cPixmap::Lock();
   pixmapLyrics->SetDrawPortPoint(cPoint(0, lyricsScrollPos * -1));
   pixmapLyrics->SetAlpha(alpha);
   cPixmap::Unlock();

   lyricsScrollPos++;
   nextScrollStep = cTimeMs::Now() + 50;

   if (lyricsScrollPos > lyricsDrawportHeight - pixmapLyrics->ViewPort().Height())
   {
      lyricsScrollingAt = time(0) + 5;
      lyricsScrollPos = 0;
   }

   return success;
}

//***************************************************************************
// Draw Symbol
//***************************************************************************

int cSqueezeOsd::drawSymbol(cPixmap* pixmap, const char* name, int& x, int y,
                            int width, int height)
{
   char* path = 0;
   cImage* image = 0;

   if (!osd)
      return fail;

   if (isEmpty(name))
      return done;

   if (width == na)
      width = pixmap->ViewPort().Width();

   if (height == na)
      height = pixmap->ViewPort().Height();

   asprintf(&path, "%s/squeezebox/%s", resDir, name);
   image = imgLoader->createImageFromFile(path, width, height, yes);

   if (image)
   {
      cPixmap::Lock();
      pixmap->DrawImage(cPoint(x, y), *image);
      cPixmap::Unlock();
   }

   x += image->Width();

   free(path);
   delete image;

   return done;
}
