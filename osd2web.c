/*
 * osd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "osd.h"
#include "service.h"

int cSqueezeOsd::sendInfoBox(TrackInfo* currentTrack)
{
   if (!osd2web)
      return done;

   osd2web->Service(SQUEEZEBOX_CURRENT_TRACK, currentTrack);

   return done;
}
