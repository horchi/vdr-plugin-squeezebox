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
#include <string>

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
      
      enum RangeQueryType
      {
         rqtUnknown = na,

         rqtGenres,
         rqtArtists,
         rqtAlbums,
         rqtNewMusic,
         rqtTracks,
         rqtYears,
         rqtPlaylists,
         rqtRadios,
         rqtRadioApps,
         rqtFavorites
      };

      struct ListItem
      {
         ListItem()     { clear(); }
         void clear()   { id = ""; content = ""; command = ""; hasItems = no; isAudio = no; }
         int isEmpty()  { return content == ""; }

         std::string id;
         std::string content;
         std::string command;
         int hasItems;
         int isAudio;
      };

      typedef std::list<ListItem> RangeList;
      typedef std::list<std::string> Parameters;

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
         char artworkTrackId[500+TB];
         char artworkurl[500+TB];
         char remoteTitle[500+TB];
         char contentType[100+TB];

         unsigned int bitrate;
         unsigned short remote;
         unsigned int year;

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

      int execute(const char* command, Parameters* pars = 0);
      int execute(const char* command, int par);
      int execute(const char* command, const char* par);

      int query(const char* command, char* response, int max);
      int queryInt(const char* command, int& value);

      int queryRange(RangeQueryType queryType, int from, int count, 
                     RangeList* list, int& total, const char* special = "", Parameters* pars = 0);

      // cover

      int getCurrentCover(MemoryStruct* cover, TrackInfo* track = 0);
      int getCover(MemoryStruct* cover, TrackInfo* track);


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
      int shuffle()        { return execute("playlist shuffle"); }
      int repeat()         { return execute("playlist repeat"); }

      int scroll(short step) 
      { 
         char par[50];
         sprintf(par, "%c%d", step < 0 ? '-' : '+', abs(step));
         return execute("time", par); 
      }

      const char* getLastQueryTitle() { return queryTitle ? queryTitle : ""; }

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

      int loadPlaylist(const char* playlist)
      { 
         char par[500];
         Parameters pars;

         pars.push_back("cmd:load");
         sprintf(par, "playlist_name:%s", playlist);
         pars.push_back(par);

         return execute("playlistcontrol", &pars);
      }

      int appendPlaylist(const char* playlist)
      { 
         char par[500];
         Parameters pars;

         pars.push_back("cmd:add");
         sprintf(par, "playlist_name:%s", playlist);
         pars.push_back(par);

         return execute("playlistcontrol", &pars);
      }

      // infos

      int update(int stateOnly = no);

      TrackInfo* getCurrentTrack() 
      { 
         if (playerState.plIndex >= 0 && playerState.plIndex < (int)tracks.size())
            return &tracks.at(playerState.plIndex);
         
         return &dummyTrack;
      }

      int hasMetadataChanged() { return metaDataChanged; }

      PlayerState* getPlayerState() { return &playerState; }

      int getTrackCount()           { return tracks.size(); }
      TrackInfo* getTrack(int idx)  { return &tracks[idx]; }

      char* unescape(char* buf);       // url like encoding
      char* escape(const char* buf);

      int request(const char* command, Parameters* par = 0);
      int request(const char* command, const char* par);

      int responseP(char*& result);
      int response(char* response = 0, int max = 0);

   private:

      void setQueryTitle(const char* title) { free(queryTitle); queryTitle = strdup(title); }

      // data

      char* host;
      unsigned short port;
      char* mac;
      char* escId;                       // escaped player id (build from mac)

      CURL* curl;
      char lastCommand[sizeMaxCommand+TB];
      std::string lastPar;

      TrackInfo dummyTrack;
      TrackInfo currentTrack;
      PlayerState playerState;
      LmcCom* notify;
      vector<TrackInfo> tracks;
      char* queryTitle;
      int metaDataChanged;

#ifdef VDR_PLUGIN
      cMutex comMutex;
#endif
};

//***************************************************************************
#endif //  __LMCCOM_H

