/*
 * lmccom.h
 *
 * See the README file for copyright information
 *
 */

#ifndef __LMCCOM_H
#define __LMCCOM_H

#include <curl/curl.h>
#include <vector>
#include <list>

using std::vector;

#include "lib/tcpchannel.h"

#ifdef VDR_PLUGIN
#  include <vdr/thread.h>
#  define LmcLock cMutexLock lock(&comMutex)
#  define LmcDoLock comMutex.Lock()
#  define LmcUnLock comMutex.Unlock()
#else
#  define LmcLock
#  define LmcDoLock
#  define LmcUnLock
#endif

//***************************************************************************
// LMC Communication
//***************************************************************************

class LmcCom : public TcpChannel
{
   public:

      typedef std::list<std::string> RangeList;

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

      struct PlayerState
      {
         PlayerState() { memset(this, 0, sizeof(PlayerState)); }

         uint64_t updatedAt;

         char mode[20+TB];
         char version[50+TB];
         int volume;
         int muted;
         int trackTime;             // playtime of the current song

         // playlist state

         char plName[300+TB];
         int plIndex;
         int plCount;
         int plShuffle;
         int plRepeat;
      };

      struct TrackInfo
      {
         TrackInfo() { memset(this, 0, sizeof(TrackInfo)); }

         uint64_t updatedAt;

         char genre[100+TB];
         char album[100+TB];
         char artist[100+TB];
         char title[200+TB];
         char coverid[500+TB];
         int index;             // index in current playlist
         int id;                // track ID
         int duration;
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

      int execute(const char* command, const char* par = "");
      int query(const char* command, char* response, int max);
      int queryRange(const char* command, int from, int count, RangeList* list, int& total);
      int queryInt(const char* command, int& value);

      // cover

      int getCurrentCover(MemoryStruct* cover);
      int getCover(MemoryStruct* cover, int trackId);


      // notification channel

      int startNotify();
      int stopNotify();
      int checkNotify(uint64_t timeout = 0);

      // player steering

      int play()           { return execute("play"); }
      int pause()          { return execute("pause", "1"); }
      int pausePlay()      { return execute("pause"); }    // toggle pause/play
      int stop()           { return execute("stop"); }
      int volumeUp()       { return execute("mixer volume", "+5"); }
      int volumeDown()     { return execute("mixer volume", "-5"); }
      int mute()           { return execute("mixer muting", "1"); }
      int unmute()         { return execute("mixer muting", "0"); }
      int muteToggle()     { return execute("mixer muting toggle"); }
      int clear()          { return execute("playlist clear"); }
      int save()           { return execute("playlist save", escId); }
      int resume()         { return execute("playlist resume", escId); }
      int randomTracks()   { return execute("randomplay tracks"); }

      int scroll(short step) 
      { 
         char par[50];
         sprintf(par, "%c%d", step < 0 ? '-' : '+', abs(step));
         return execute("time", par); 
      }

      int nextTrack()      { return execute("playlist index", "+1"); }
      int prevTrack()      { return execute("playlist index", "-1"); }

      int track(unsigned short index)
      { 
         char par[50];
         sprintf(par, "%d", index);
         return execute("playlist index", par);
      }

      int loadAlbum(const char* genre = "*", const char* artist = "*", const char* album = "*")
      { 
         return ctrlAlbum("loadalbum", genre, artist, album);
      }

      int appendAlbum(const char* genre = "*", const char* artist = "*", const char* album = "*")
      { 
         return ctrlAlbum("addalbum", genre, artist, album);
      }

      int ctrlAlbum(const char* command, const char* genre = "*", const char* artist = "*", const char* album = "*")
      { 
         int status;
         char *a = 0, *g = 0, *l = 0;
         char* cmd;

         asprintf(&cmd, "playlist %s %s %s %s", command, 
                  *genre == '*' ? genre : g = escape(genre), 
                  *artist == '*' ? artist : a = escape(artist), 
                  *album == '*' ? album : l = escape(album));

         status = execute(cmd);
         free(cmd); free(a); free(g); free(l);
         return status;
      }

      int loadPlaylist(const char* playlist)     { return ctrlPlaylist("load", playlist); }
      int appendPlaylist(const char* playlist)   { return ctrlPlaylist("add", playlist); }

      int ctrlPlaylist(const char* command, const char* playlist)
      { 
         int status;
         char* cmd; 
         asprintf(&cmd, "playlistcontrol cmd:%s playlist_name:%s", command, escape(playlist));
         status = execute(cmd);
         return status;
      }
      
      // infos

      int update(int stateOnly = no);

      TrackInfo* getCurrentTrack() 
      { 
         if (playerState.plIndex >= 0 && playerState.plIndex < (int)tracks.size())
            return &tracks.at(playerState.plIndex);

         
         return &dummyTrack;
      }


      PlayerState* getPlayerState() { return &playerState; }

      int getTrackCount()          { return tracks.size(); }
      TrackInfo* getTrack(int idx) { return &tracks[idx]; }

      char* unescape(char* buf);       // url like encoding
      char* escape(const char* buf);

   private:

      int request(const char* command, const char* par = "");
      int response(char* response = 0, int max = 0);

      // data

      char* host;
      unsigned short port;
      char* mac;
      char* escId;                       // escaped player id (build from mac)

      CURL* curl;
      char lastCommand[sizeMaxCommand+TB];
      char* lastPar;

      TrackInfo dummyTrack;
      TrackInfo currentTrack;
      PlayerState playerState;
      LmcCom* notify;
      vector<TrackInfo> tracks;

#ifdef VDR_PLUGIN
      cMutex comMutex;
#endif
};

//***************************************************************************
#endif //  __LMCCOM_H

