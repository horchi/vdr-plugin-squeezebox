/*
 * test.c
 *
 * See the README file for copyright information
 *
 */

#include <sys/wait.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "lib/common.h"
#include "lmccom.h"
#include "lmctag.h"

int doShutdown = no;

//***************************************************************************
// Usage
//***************************************************************************

void showUsage(const char* name)
{
   printf("Usage: %s [-l <log-level>] [-h <host>] [-p port]\n", name);
   printf("    -l <log-level>  set log level\n");
   printf("    -h <LMC-host>   \n");
   printf("    -p <LMC-port>   \n");
}

//***************************************************************************
// Sognal Handler
//***************************************************************************

void downF(int signal)     
{ 
   tell(0, "Shutdown triggered with signal %d", signal); 
   doShutdown = yes; 
}

void showTags(char* buf)
{
   tell(0, "\n\n%s", buf);

   free(buf);

   return ;
}

//***************************************************************************
// Main
//***************************************************************************

int main(int argc, char** argv)
{
   const char* lmcHost = "localhost";
   const char* command = 0;
   const char* parameter = 0;
   unsigned short lmcPort = 9090;
   char* mac = getMac();
   MemoryStruct cover;

   logstdout = yes;
   loglevel = 1;

   LmcCom::RangeList list;
   LmcCom* lmc = new LmcCom(mac);
   LmcCom::TrackInfo* track = 0;
   LmcCom::PlayerState* player;

   // Usage ..

   if (argc > 1 && (argv[1][0] == '?' || (strcmp(argv[1], "--help") == 0)))
   {
      showUsage(argv[0]);
      return 0;
   }

   // Parse command line

   for (int i = 0; argv[i]; i++)
   {
      if (argv[i][0] != '-' || strlen(argv[i]) != 2)
         continue;

      switch (argv[i][1])
      {
         case 'l': if (argv[i+1]) loglevel = atoi(argv[++i]); break;
         case 'h': if (argv[i+1]) lmcHost = argv[++i];        break;
         case 'p': if (argv[i+1]) lmcPort = atoi(argv[++i]);  break;
         case 'c': if (argv[i+1]) command = argv[++i];        break;
         case 'P': if (argv[i+1]) parameter = argv[++i];      break;
         case 'e': 
            showTags(lmc->unescape(strdup(argv[i+1])));
            goto EXIT;
         default: 
         {
            showUsage(argv[0]);
            return 0;
         }
      }
   }

   // register signals

//    ::signal(SIGINT, downF);
//    ::signal(SIGTERM, downF);

   // open LMC connection

   if (lmc->open(lmcHost, lmcPort) != success)
   {
      tell(0, "Opening connection to LMC server at '%s:%d' failed", lmcHost, lmcPort);
      delete lmc; lmc = 0;
      return fail;
   }

   tell(0, "mac ist: '%s'", mac);

   // ===================================================================

   if (!isEmpty(command))
   {
      int status;
      LmcCom::RangeList list;
      char* result = 0;
      char cmd[200];   *cmd = 0;
      LmcTag lt(lmc);
      int tag;
      const int maxValue = 100;
      char value[maxValue+TB];
      LmcCom::Parameters params;

      if (!isEmpty(parameter))
      {
         params.push_back(parameter);
      }

      snprintf(cmd, 200, "%s %d %d", command, 0, 3);

      status = lmc->request(cmd, &params);
      status += lmc->write("\n");
      
      if ((status += lmc->responseP(result)) != success || isEmpty(result))
      {
         free(result);
         tell(eloAlways, "Error: Request of '%s' failed", cmd);
         return status;
      }
      
      lt.set(result);   
      tell(eloDebug, "Got [%s]", lmc->unescape(result));
      free(result); result = 0;

      while (lt.getNext(tag, value, maxValue) != LmcTag::wrnEndOfPacket)
      {
         tell(0, "'%s' - '%s'", LmcTag::toName(tag), value);
      }
      
      return 0;
   }

   // update server state

   tell(0, "--------------------------");
   lmc->update();
   player = lmc->getPlayerState();

   lmc->repeat();
   lmc->repeat();
   lmc->repeat();

   if (strcmp(player->mode, "stop") == 0)
   {
//      tell(0, "--------------------------");
//       lmc->randomTracks();
//       lmc->updateTrackList();
   }

   // lmc->nextTrack();

   tell(0, "--------------------------");
   tell(0, "Connection to LMC server version '%s' at '%s:%d' established", player->version, lmcHost, lmcPort);
   tell(0, "Player: mode %s; volume %d; muted %s, currend track index %d", 
        player->mode, player->volume, player->muted ? "yes" : "no", player->plIndex);

   track = lmc->getCurrentTrack();

   if (track)
   {
      time_t time = player->trackTime + (cTimeMs::Now() - track->updatedAt) / 1000;
      time_t remaining = track->duration - time;

      tell(0, "--------------------------");
      tell(0, "Titel: %s", track->title);
      tell(0, "Interpret: %s", track->artist);
      tell(0, "Genre: %s", track->genre);
      tell(0, "Dauer: %d:%02d", track->duration / tmeSecondsPerMinute, track->duration % tmeSecondsPerMinute);
      tell(0, "Progress: %d:%02d ... -%d:%02d", 
           time / tmeSecondsPerMinute, time % tmeSecondsPerMinute,
           remaining / tmeSecondsPerMinute, remaining % tmeSecondsPerMinute);
   }

   // --------------------------
   // query range

   int total;

   tell(0, "--------------------------");
   tell(0, "Requesting 'genres': ");

   if (lmc->queryRange(LmcCom::rqtGenres, 0, 100,  &list, total) == success)
   {
      LmcCom::RangeList::iterator it;
      
      for (it = list.begin(); it != list.end(); ++it)
         tell(0, "  '%s'", (*it).content.c_str());
      
      if (total > 100)
         tell(eloAlways, "Warning: [%s] %d more, only 100 supported", "genres", total-100);
   }

   tell(0, "--------------------------");
   tell(0, "Requesting 'radios': ");

   if (lmc->queryRange(LmcCom::rqtRadios, 0, 100,  &list, total) == success)
   {
      LmcCom::RangeList::iterator it;
      
      for (it = list.begin(); it != list.end(); ++it)
         tell(0, "  '%s' - '%s'", (*it).content.c_str(), (*it).command.c_str());
      
      if (total > 100)
         tell(eloAlways, "Warning: [%s] %d more, only 100 supported", "genres", total-100);
   }

   // tell(0, "--------------------------");
   // tell(0, "Load Album");
   // lmc->loadAlbum("Classic Rock", "*", "*");

   // --------------------------
   // tracklist (playlist)

   if (yes)
   {
      tell(0, "--------------------------");
      // lmc->updateTrackList();
      tell(0, "Playlist: '%s'", player->plName);
      
      for (int i = 0; i < lmc->getTrackCount(); i++)
         tell(0, "  (%d) '%s' - '%s'", i,
              lmc->getTrack(i)->artist, lmc->getTrack(i)->title);
   }
   
  EXIT:

   // close connection 

   tell(0, "Stopping notification, closing LMC connection");

   lmc->stopNotify();

   lmc->close();
   delete lmc;
   free(mac);

   return success;
}
