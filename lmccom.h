/*
 * lmccom.h
 *
 * See the README file for copyright information
 *
 */

#include <curl/curl.h>

#include "lib/tcpchannel.h"

//***************************************************************************
// LMC Communication
//***************************************************************************

class LmcCom : public TcpChannel
{
   public:

      enum Misc
      {
         sizeMaxCommand = 100
      };

      enum Results
      {
         wrnNoEventPending = -1000,
         ifoTrack,
         ifoPlayer
      };

      struct PlayerInfo
      {
         char mode[20+TB];
         char id[50+TB];
         char version[50+TB];
         int volume;
         int muted;

         // playlist state

         int plIndex;
         int plCount;
         int plShuffle;
         int plRepeat;
      };

      struct TrackInfo
      {
         uint64_t updatedAt;

         char genre[100+TB];
         char album[100+TB];
         char artist[100+TB];
         char title[200+TB];
         int duration;
         int time;
         int remaining;

         char path[500+TB];
      };

      LmcCom(const char* aMac = 0);
      ~LmcCom();

      void setMac(const char* aMac) 
      { 
         free(mac); 
         mac = strdup(aMac); 
         free(escId); 
         escId = escape(mac);
      }

      int open(const char* host = "localhost", unsigned short port = 9090);

      int execute(const char* command);
      int query(const char* command, char* response, int max);
      int queryInt(const char* command, int& value);

      // cover

      int getCurrentCover(MemoryStruct* cover);

      // notification channel

      int startNotify();
      int stopNotify();
      int checkNotify();

      // player steering

      int play()           { return execute("play"); }
      int pause()          { return execute("pause 1"); }
      int pausePlay()      { return execute("pause"); }    // toggle pause/play
      int stop()           { return execute("stop"); }
      int volumeUp()       { return execute("mixer volume +5"); }
      int volumeDown()     { return execute("mixer volume -5"); }
      int mute()           { return execute("mixer muting 1"); }
      int unmute()         { return execute("mixer muting 0"); }
      int muteToggle()     { return execute("mixer muting toggle"); }

      int scroll(short step) 
      { 
         char cmd[50];
         sprintf(cmd, "time %c%d", step < 0 ? '-' : '+', abs(step));
         return execute("cmd"); 
      }

      int nextTrack()      { return execute("playlist index +1"); }
      int prevTrack()      { return execute("playlist index -1"); }

      int track(unsigned short index)
      { 
         char cmd[50];
         sprintf(cmd, "playlist index %d", index);
         return execute("cmd"); 
      }

      // url like encoding

      char* unescape(char* buf);
      char* escape(char* buf);
      
      // infos

      TrackInfo* getCurrentTrack() { return &currentTrack; }
      TrackInfo* updateCurrerntTrack();
      PlayerInfo* updatePlayerState();
      PlayerInfo* getPlayerState() { return &playerState; }
      
   private:

      int request(const char* command);
      int response(char* response = 0, int max = 0);

      // data

      char* host;
      unsigned short port;
      char* mac;
      char* escId;                       // escaped player id (build from mac)

      CURL* curl;
      char lastCommand[sizeMaxCommand+TB];
      TrackInfo currentTrack;
      PlayerInfo playerState;
      LmcCom* notify;
};
