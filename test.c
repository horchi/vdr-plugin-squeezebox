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

//***************************************************************************
// Main
//***************************************************************************

int main(int argc, char** argv)
{
   const char* lmcHost = "localhost";
   unsigned short lmcPort = 9090;
   char* mac = getMac();
   MemoryStruct cover;

   logstdout = yes;
   loglevel = 1;

   LmcCom* lmc = new LmcCom(mac);
   LmcCom::TrackInfo* track;
   LmcCom::PlayerInfo* player;

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

         default: 
         {
            showUsage(argv[0]);
            return 0;
         }
      }
   }

   // register signals

   ::signal(SIGINT, downF);
   ::signal(SIGTERM, downF);

   // open LMC connection

   if (lmc->open(lmcHost, lmcPort) != success)
   {
      tell(0, "Opening connection to LMC server at '%s:%d' failed", lmcHost, lmcPort);
      delete lmc;
      lmc = 0;
      return fail;
   }

   // update server state

   player = lmc->updatePlayerState();  
   track = lmc->updateCurrerntTrack();

   tell(0, "Connection to LMC server version '%s' at '%s:%d' established", player->version, lmcHost, lmcPort);
   tell(0, "Player: id '%s'; volume %d; muted %s", player->id, player->volume, player->muted ? "yes" : "no");

   if (track)
   {
      time_t time = track->time + (cTimeMs::Now() - track->updatedAt) / 1000;
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

//    // next song ...

//    lmc->execute("playlist index +1");
//    lmc->query("title", title, 100);
//    tell(0, "%s", title);

   // enable notification

   lmc->startNotify();

   // ....

   while (!doShutdown)
   {
      int notification = lmc->checkNotify();

      if (notification == LmcCom::ifoTrack)
      {
         track = lmc->getCurrentTrack();
         
         if (track)
         {
            time_t time = track->time + (cTimeMs::Now() - track->updatedAt) / 1000;
            time_t remaining = track->duration - time;

            tell(0, "--------------------------");
            tell(0, "Titel: %s", track->title);
            tell(0, "Interpret: %s", track->artist);
            tell(0, "Genre: %s", track->genre);
            tell(0, "Dauer: %d:%02d", track->duration / tmeSecondsPerMinute, track->duration % tmeSecondsPerMinute);
            tell(0, "Progress: %d:%02d ... -%d:%02d", 
                 time / tmeSecondsPerMinute, time % tmeSecondsPerMinute,
                 remaining / tmeSecondsPerMinute, remaining % tmeSecondsPerMinute);

            lmc->getCurrentCover(&cover);
            storeFile(&cover, "cover.jpg");
            tell(eloAlways, "Saved cover to 'cover.jpg'");
         }
      }
      else if (notification == LmcCom::ifoPlayer)
      {
         tell(0, "Player mode now: %s", player->mode);
      }
      else if (strcmp(player->mode, "play") == 0)
      {
         time_t time = track->time + (cTimeMs::Now() - track->updatedAt) / 1000;
         time_t remaining = track->duration - time;

         tell(0, "Progress: %d:%02d ... -%d:%02d (vol %d %s)", 
              time / tmeSecondsPerMinute, time % tmeSecondsPerMinute,
              remaining / tmeSecondsPerMinute, remaining % tmeSecondsPerMinute,
              player->volume, player->muted ? "yes" : "no"
            );
      }
   }

   // close connection 

   tell(0, "Stopping notification, closing LMC connection");

   lmc->stopNotify();

   lmc->close();
   delete lmc;
   free(mac);

   return success;
}
