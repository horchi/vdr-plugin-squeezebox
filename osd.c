/*
 * osd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include <vdr/keys.h>

#include "lib/common.h"

#include "lmccom.h"
#include "config.h"
#include "osd.h"
#include "helpers.h"

//***************************************************************************
// Squeeze Osd
//***************************************************************************

cSqueezeOsd::cSqueezeOsd() 
   : cThread()
{
   loopActive = no;
   visible = no;
   forceNextDraw = yes;
   border = 10;
   plTop = 0;
   plCurrent = 0;              // current user selected item, or current track (if no user action)
   lastScrollAt = time(0);
   plItemSpace = 10;
   plItems = 0;
   plItemHeight = 0;

   osd = 0;

   memset(pixmapInfo, 0, sizeof(pixmapInfo));
   memset(pixmapPlaylist, 0, sizeof(pixmapPlaylist));
   memset(pixmapPlCurrent, 0, sizeof(pixmapPlCurrent));
   memset(pixmapStatus, 0, sizeof(pixmapStatus));
   memset(pixmapBtnRed, 0, sizeof(pixmapBtnRed));
   memset(pixmapBtnGreen, 0, sizeof(pixmapBtnGreen));
   memset(pixmapBtnYellow, 0, sizeof(pixmapBtnYellow));
   memset(pixmapBtnBlue, 0, sizeof(pixmapBtnBlue));

   fontStd = 0;
   fontTilte = 0;
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
   hide();

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
   
   delete osd;
   delete fontTilte;
   delete fontStd;
   delete fontPl;
   delete lmc;
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
   // visible = no;
   forceNextDraw = yes;
}

//***************************************************************************
// Process Key
//***************************************************************************

int cSqueezeOsd::ProcessKey(int key)
{
   
   switch (key)
   {       
      case kUp:
      {
         if (plCurrent > 0)
            plCurrent--;

         lastScrollAt = time(0);
         forceNextDraw = yes;

         break;
      }

      case kDown:  
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent++;

         lastScrollAt = time(0);
         forceNextDraw = yes;

         break;
      }

      case kRight:
      {
         if (plCurrent < lmc->getTrackCount()-1)
            plCurrent = min(plCurrent+plItems, lmc->getTrackCount()-1);

         lastScrollAt = time(0);
         forceNextDraw = yes;

         break;
      }

      case kLeft:
      {
         if (plCurrent > 0)
            plCurrent = max(plCurrent-plItems, 0);

         lastScrollAt = time(0);
         forceNextDraw = yes;

         break;
      }

      case kOk: 
      {
         if (plCurrent > na && plCurrent < lmc->getTrackCount()-1) 
            lmc->track(plCurrent);

         break;
      }
   }

   return done;
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

      if (time(0) > lastScrollAt + 10)
      {
         plCurrent = currentState->plIndex;
         plTop = max(0, currentState->plIndex-2);
         forceNextDraw = yes;
      }

      // check for notification with 100ms timeout

      changesPending = lmc->checkNotify(100) == success;

      if (!visible)
         continue;

      fullDraw = forceNextDraw || changesPending;

      if (osd && (cTimeMs::Now() > lastDraw+1000 || fullDraw))
      {
         if (yes)  // fullDraw) - sice we don't notice when we lost or get the focus we need a full draw ervery time :(
            drawOsd();
         else
            drawProgress();

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
         tell(0, "Can't open osd, CanHandleAreas()");
         return fail;
      }

      osd->SetAreas(&control, 1);

      // create fonts

      const cFont* vdrFont = cFont::GetFont(fontOsd);

      fontStd = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 9.0);
      fontTilte = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 13.0);
      fontPl = cFont::CreateFont(vdrFont->FontName(), (vdrFont->Height() / 10.0) * 7.0);

      // draw black background

      osd->DrawRectangle(0, 0, cOsd::OsdWidth(), cOsd::OsdHeight(), clrBlack);

      // create boxed

      int stHeight = fontStd->Height() + 2 * border;
      int ifoHeight = 2 * fontTilte->Height() + 4 * fontStd->Height() + 2 * border;
      int plY = border + ifoHeight + 2 * border;
      int plHeight = cOsd::OsdHeight() - plY - stHeight - 2 * border;
      int stY = plY + plHeight + 2 * border;
      int btnWidth = width / 4 - border;
      int btnX = border;

      plItems = (plHeight-2*border) / (fontPl->Height()*2 + plItemSpace);
      plItemSpace = ((plHeight-2*border) - (plItems * fontPl->Height()*2)) / plItems;
      plItemHeight = fontPl->Height()*2 + plItemSpace;

      tell(0, "calculated %d items with a space of %d, hight is %d", 
           plItems, plItemSpace, (plHeight-2*border));

      createBox(pixmapInfo, leftX, border, width, ifoHeight, clrBox, clrBoxBlend);
      createBox(pixmapPlaylist, leftX, plY, width, plHeight, clrBox, clrBoxBlend);
      createBox(pixmapPlCurrent, leftX, plY, width, plItemHeight, clrBox, clrWhite);
      createBox(pixmapStatus, leftX, stY, width, stHeight, clrBox, clrBoxBlend);

      createBox(pixmapBtnRed, btnX, stY, btnWidth, stHeight, 0xFF990000, 0xFFFF0000);
      btnX += btnWidth + border;
      createBox(pixmapBtnGreen, btnX, stY, btnWidth, stHeight, 0xFF009900, 0xFF00EE00);
      btnX += btnWidth + border;
      createBox(pixmapBtnYellow, btnX, stY, btnWidth, stHeight, 0xFF999900, 0xFFEEEE00);
      btnX += btnWidth + border;
      createBox(pixmapBtnBlue, btnX, stY, btnWidth, stHeight, clrBox, clrBoxBlend);
   }

   return done;
}

//***************************************************************************
// Create Box
//***************************************************************************

int cSqueezeOsd::createBox(cPixmap* pixmap[], int x, int y, int width, int height, 
                           tColor color, tColor blend)
{
   cPixmap::Lock();

   // background pixmap

   pixmap[pmBack] = osd->CreatePixmap(1, cRect(x, y, width, height));
   pixmap[pmBack]->Fill(color);
   DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, true);
   DrawBlendedBackground(pixmap[pmBack], 0, width, color, blend, false);
   DrawRoundedCorners(pixmap[pmBack], 25, 0, 0, width, height);

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
   // set alphy to force redraw of background boxes :(

   pixmapInfo[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlaylist[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapPlCurrent[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapStatus[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnRed[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnGreen[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnYellow[pmBack]->SetAlpha(ALPHA_TRANSPARENT);
   pixmapBtnBlue[pmBack]->SetAlpha(ALPHA_TRANSPARENT);

   // draw ...

   drawCover();
   drawInfoBox();
   drawPlaylist();
   drawStatus();
   drawButtons();

   return success;
}

//***************************************************************************
// Draw Info Box
//***************************************************************************

int cSqueezeOsd::drawInfoBox()
{
   LmcCom::TrackInfo* currentTrack = lmc->getCurrentTrack();
   int step = fontStd->Height();

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

   pixmapInfo[pmText]->DrawText(cPoint(x, y), currentTrack->artist, clrWhite, clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
   pixmapInfo[pmText]->DrawText(cPoint(x, y+=step), cString::sprintf("Genre: %s", currentTrack->genre), clrWhite, 
                                clrTransparent, fontStd, pixmapInfo[pmText]->ViewPort().Width());
   pixmapInfo[pmText]->DrawText(cPoint(x, y+=step), cString::sprintf("(%d / %d)", currentState->plIndex+1, currentState->plCount), 
                                clrTextDark, clrTransparent, fontPl, pixmapInfo[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   drawProgress(y+fontPl->Height());

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

   LmcCom::TrackInfo* currentTrack = lmc->getCurrentTrack();

   if (y != na)  yLast = y;

   if (strcmp(currentState->mode, "play") != 0 || !currentTrack || !currentTrack->duration)
      return done;

   const cRect rect = pixmapInfo[pmText]->ViewPort();

   int total = currentTrack->duration;
   int time = currentState->trackTime + (cTimeMs::Now() - currentState->updatedAt) / 1000;
   int remaining = total - time;

   cString begin = cString::sprintf("%d:%02d", time/tmeSecondsPerMinute, time%tmeSecondsPerMinute);
   cString end = cString::sprintf("-%d:%02d", remaining/tmeSecondsPerMinute, remaining%tmeSecondsPerMinute);

   int barX = fontStd->Width(begin) + 10;
   int barWidth = rect.Width() - barX - fontStd->Width(end) - 20;

   double percent = time / (total / 100.0);
   int off = barWidth - ((barWidth / 100.0) * percent);
   int barHeight = fontStd->Height();

   // crear area

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

   cPixmap::Lock();

   pixmapPlaylist[pmText]->Fill(clrTransparent);       // clear box

   if (lmc->getTrackCount() != lastCount)
      plCurrent = currentState->plIndex;

   lastCount = lmc->getTrackCount();

   if (plCurrent < plTop)
      plTop = plCurrent;
   else if (plCurrent >= plTop + plItems)
      plTop = plCurrent - plItems +1;

   for (int i = plTop, cnt = 0; i < lmc->getTrackCount() && cnt < plItems; i++, cnt++)
   {
      tColor color = clrTextDark;

      if (i == currentState->plIndex)
         color = clrWhite;

      if (i == plCurrent)
      {
         int ay = pixmapPlaylist[pmBack]->ViewPort().Y() + y;

         pixmapPlCurrent[pmBack]->SetViewPort(cRect(pixmapPlCurrent[pmBack]->ViewPort().X(), ay, 
                                                    pixmapPlCurrent[pmBack]->ViewPort().Width(), 
                                                    pixmapPlCurrent[pmBack]->ViewPort().Height()));
      }
         
      pixmapPlaylist[pmText]->DrawText(cPoint(0, y),
                                       cString::sprintf("%s",lmc->getTrack(i)->title),
                                       color, clrTransparent, fontPl, pixmapPlaylist[pmText]->ViewPort().Width());

      y += fontPl->Height();

      pixmapPlaylist[pmText]->DrawText(cPoint(0, y),
                               cString::sprintf("%s", lmc->getTrack(i)->artist), 
                               color, clrTransparent, fontPl, pixmapPlaylist[pmText]->ViewPort().Width());

      y += fontPl->Height()+plItemSpace;
   }

   pixmapPlaylist[pmBack]->SetAlpha(ALPHA_OPAQUE);
   pixmapPlaylist[pmText]->SetAlpha(ALPHA_OPAQUE);
   pixmapPlCurrent[pmBack]->SetAlpha(ALPHA_OPAQUE);

   cPixmap::Unlock();

   return done;
}

//***************************************************************************
// Draw Info Box
//***************************************************************************

int cSqueezeOsd::drawStatus()
{
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

   pixmapBtnRed[pmText]->DrawText(cPoint(0, 0), "Menu", 
                                  clrWhite, clrTransparent, fontStd, 
                                  pixmapBtnRed[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   pixmapBtnGreen[pmText]->DrawText(cPoint(0, 0), "<<", 
                                    clrWhite, clrTransparent, fontStd, 
                                    pixmapBtnGreen[pmText]->ViewPort().Width(), 0, taCenter | taTop);
   
   pixmapBtnYellow[pmText]->DrawText(cPoint(0, 0), ">>", 
                                     clrWhite, clrTransparent, fontStd, 
                                     pixmapBtnYellow[pmText]->ViewPort().Width(), 0, taCenter | taTop);

   pixmapBtnBlue[pmText]->DrawText(cPoint(0, 0), currentState->plCount ? "Clear" : "Random", 
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
      unsigned short imgWidth = cOsd::OsdWidth() / 2 - 2 * border;

      cImage* image = imgLoader->createImage(imgWidth, imgWidth, yes);

      if (image)
      {
         osd->DrawImage(cPoint(border, border), *image);
         delete image;
      }
   }

   return success;
}