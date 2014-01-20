/*
 * lmccom.c
 *
 * See the README file for copyright information
 *
 */

#ifdef VDR_PLUGIN
# include <vdr/tools.h>
#endif

#include "lib/common.h"
#include "lmccom.h"
#include "lmctag.h"

//***************************************************************************
// Object
//***************************************************************************

LmcCom::LmcCom(const char* aMac)
   : TcpChannel()
{
   // init curl, used only for (de)escape the url encoding

   curl_global_init(CURL_GLOBAL_NOTHING);
   curl = curl_easy_init();

   notify = 0;
   port = 0;
   host = 0;
   mac = 0;
   escId = 0;

   *lastCommand = 0;
   lastPar = 0;

   if (!isEmpty(aMac))
   {
      mac = strdup(aMac);
      escId = escape(mac);
   }
}

LmcCom::~LmcCom()
{ 
   if (notify) stopNotify();
      
   curl_easy_cleanup(curl);
   close();
   free(host);
   free(mac);
   free(escId);
   free(lastPar);
}

//***************************************************************************
// Open
//***************************************************************************

int LmcCom::open(const char* aHost, unsigned short aPort)
{
   LmcLock;

   free(host);
   port = aPort;
   host = strdup(aHost);

   return TcpChannel::open(port, host);
}

//***************************************************************************
// Update Current Playlist
//***************************************************************************

int LmcCom::update(int stateOnly)
{
   LmcLock;

   const int maxValue = 500;    // 500 due to long url tag
   char value[maxValue+TB];
   int tag;

   LmcTag lt(this);
   char cmd[100];
   int status = success;
   int count = 0;
   TrackInfo t;
   int track = no;

   memset(&playerState, 0, sizeof(playerState));
   tracks.clear();

   status += queryInt("playlist tracks", count);
   query("version",               playerState.version,  sizeof(playerState.version));
   queryInt("mixer muting",       playerState.muted);

   if (status != success)
      return status;

   int size = 500 + 600 * count;
   char* buf = (char*)malloc(size);

   if (!stateOnly)   
      count = max(count, 100);

   char* param = escape("tags:agdclu");
   sprintf(cmd, "status 0 %d %s", count, param);
   free(param);

   // perform LMC request ..

   LmcDoLock;
   status = request(cmd);
   status += write("\n");
   status += response(buf, size);
   LmcUnLock;

   if (status != success)
   {
      tell(eloAlways, "Error: Request of '%s' failed", cmd);
      free(buf);
      return fail;
   }

   lt.set(buf);
   free(buf);

   t.index = na;
   
   while (lt.getNext(tag, value, maxValue, track) != LmcTag::wrnEndOfPacket)
   {
      switch (tag)
      {
         case LmcTag::tTime:             playerState.trackTime = atoi(value);  playerState.updatedAt = cTimeMs::Now(); break;
         case LmcTag::tMixerVolume:      playerState.volume = atoi(value);     break;
         case LmcTag::tPlaylistCurIndex: playerState.plIndex  = atoi(value);   break;
         case LmcTag::tPlaylistShuffle:  playerState.plShuffle = atoi(value);  break;
         case LmcTag::tPlaylistRepeat:   playerState.plRepeat = atoi(value);   break;
         case LmcTag::tMode:             snprintf(playerState.mode, sizeof(playerState.mode), "%s", value);     break;
         case LmcTag::tPlaylistName:     snprintf(playerState.plName, sizeof(playerState.plName), "%s", value); break;

         // playlist tags ...

         case LmcTag::tPlaylistIndex:
         {
            if (t.index != na)
            {
               t.updatedAt = cTimeMs::Now();
               tracks.push_back(t);
               memset(&t, 0, sizeof(t));
            }
            
            track = yes;
            t.index = atoi(value);
            break;
         }
         
         case LmcTag::tId:            t.id = atoi(value);                                  break;
         case LmcTag::tTitle:         snprintf(t.title, sizeof(t.title), "%s", value);     break;
         case LmcTag::tArtist:        snprintf(t.artist, sizeof(t.artist), "%s", value);   break;
         case LmcTag::tGenre:         snprintf(t.genre, sizeof(t.genre), "%s", value);     break;
         case LmcTag::tTrackDuration: t.duration = atoi(value);                            break;  
         case LmcTag::tCoverid:       snprintf(t.coverid, sizeof(t.coverid), "%s", value); break; 
         case LmcTag::tAlbum:         snprintf(t.album, sizeof(t.album), "%s", value);     break;
      // case LmcTag::tUrl:           snprintf(t.url, sizeof(t.url), "%s", url);           break;
      }
   }

   if (t.index != na)
   {
      t.updatedAt = cTimeMs::Now();
      tracks.push_back(t);
   }

   playerState.plCount = tracks.size();
   tell(eloDetail, "playlist updated, got %d track", playerState.plCount);

   return success;
}

//***************************************************************************
// Query
//***************************************************************************

int LmcCom::query(const char* command, char* result, int max)
{
   LmcLock;

   int status;

   status = request(command);
   status += write(" ?\n");

   if ((status += response(result, max)) != success)
       tell(eloAlways, "Error: Request of '%s' failed", command);

   unescape(result);

   return status;
}

int LmcCom::queryInt(const char* command, int& value)
{
   LmcLock;

   // LogDuration ld("queryInt", 0);
   int status;
   char result[100+TB];

   status = request(command);
   status += write(" ?\n");
   status += response(result, 100);

   unescape(result);

   if (status == success)
      value = atol(result);

   return status;
}

//***************************************************************************
// Query Range
//***************************************************************************

int LmcCom::queryRange(const char* command, int from, int count, RangeList* list, int& total)
{
   LmcLock;

   const int maxResult = 10000;
   const int maxValue = 100;
   char value[maxValue+TB];
   char content[maxValue+TB]; *content = 0;
   int tag;
   int id = na;
   char result[maxResult+TB];
   char cmd[200];
   LmcTag lt(this);
   int status;

   snprintf(cmd, 200, "%s %d %d", command, from, count);
   status = request(cmd);
   status += write("\n");
   total = 0;

   if ((status += response(result, maxResult)) != success || isEmpty(result))
   {
      tell(eloAlways, "Error: Request of '%s' failed", command);
      return status;
   }

   lt.set(result);   
   tell(eloDetail, "Got [%s]", unescape(result));

   while (lt.getNext(tag, value, maxValue) != LmcTag::wrnEndOfPacket)
   {
      switch (tag)
      {
         case LmcTag::tItemCount: total = atoi(value);     break;
         case LmcTag::tGenre:     strcpy(content, value);  break;
         case LmcTag::tArtist:    strcpy(content, value);  break;
         case LmcTag::tAlbum:     strcpy(content, value);  break;
         case LmcTag::tPlaylist:  
            if (strcmp(value, escId) != 0)
               strcpy(content, value);  
            break;
         case LmcTag::tId:        id = atoi(value);        break;
      }

      if (id != na && !isEmpty(content))
      {
         list->push_back(content);
         *content = 0;
         id = na;
      }
   }

   list->sort();
   
   return success;
}

//***************************************************************************
// Execute
//***************************************************************************

int LmcCom::execute(const char* command, const char* par)
{
   LmcLock;

   tell(eloDetail, "exectuting '%s' with '%s'", command, par ? par : "");
   request(command, par);
   write("\n");

   return response();
}

int LmcCom::execute(const char* command, int par)
{
   LmcLock;
   char str[10];

   sprintf(str, "%d", par);
   
   return execute(command, str);
}

//***************************************************************************
// Request
//***************************************************************************

int LmcCom::request(const char* command, const char* par)
{
   int status;
   
   free(lastPar); lastPar = 0;
   snprintf(lastCommand, sizeMaxCommand, "%s", command);

   if (!isEmpty(par))
      lastPar = escape(par);
   
   tell(eloDebug, "requsting '%s' with '%s'", 
        lastCommand, lastPar);

   flush();

   status = write(escId)
      + write(" ")
      + write(lastCommand);

   if (lastPar)
      status += write(" ")
         + write(lastPar);

   return status;
}

//***************************************************************************
// Response
//***************************************************************************

int LmcCom::response(char* result, int max)
{
   // LogDuration ld("response", 0);

   int status = success;
   char* buf = 0;

   if (result)
      *result = 0;

   if (look(30000) == success && (buf = readln()))
   {
      char* p = buf + strlen(escId) +1;
      
      if ((p = strstr(p, lastCommand)) && strlen(p) >= strlen(lastCommand))
      {
         p += strlen(lastCommand);

         while (*p && *p == ' ')         // skip leading blanks
            p++;
         
         if (result && max && strcmp(p,"?") != 0)
            sprintf(result, "%.*s", max, p);
         
         if (loglevel >= eloDebug)
            tell(eloDebug, "<- (response %d bytes) [%s]", strlen(buf), unescape(p));
      }
      else
      {
         tell(eloAlways, "Got unexpected answer for '%s' [%s]", 
              lastCommand, buf);
         
         status = fail;
      }
      
      free(buf);
   }

   return status;
}

//***************************************************************************
// /Un)escape URL Encoding
//***************************************************************************

char* LmcCom::unescape(char* buf)
{
   int len = 0;
   char* res;

   res = curl_easy_unescape(curl , buf, strlen(buf), &len);
   
   if (res)
      strcpy(buf, res);

   free(res);

   return buf;
}

char* LmcCom::escape(const char* buf)
{
   char* res;

   res = curl_easy_escape(curl , buf, strlen(buf));
   
   return res;
}

//***************************************************************************
// Notification
//***************************************************************************

int LmcCom::startNotify()
{
   notify = new LmcCom(mac);

   if (notify->open(host, port) != success)
   {
      delete notify;
      notify = 0;
      return fail;
   }

   return notify->execute("listen 1");
}

int LmcCom::stopNotify()
{
   if (notify)
   {
      notify->execute("listen 0");
      notify->close();
      delete notify;
      notify = 0;
   }

   return success;
}

//***************************************************************************
// Check for Notification
//***************************************************************************

int LmcCom::checkNotify(uint64_t timeout)
{
   // LogDuration ld("checkNotify", 0);
   char buf[1000+TB];
   int status = wrnNoEventPending;

   if (!notify)
   {
      tell(eloAlways, "Cant check for notifications until startNotify is called");
      return fail;
   }

   while (notify->look(timeout) == success)
   {
      if (notify->read(buf, 1000, yes) == success)
      {
         buf[strlen(buf)-1] = 0;   // cut LF

         unescape(buf);
       
         tell(eloDetail, "<- [%s]", buf);
         
         if (strstr(buf, "playlist "))
            status = success;
         else if (strstr(buf, "pause ") || strstr(buf, "server"))
            status = success;
      }
   }

   if (status == success)
      update();

   return status;
}

//***************************************************************************
// Get Current Cover
//***************************************************************************

int LmcCom::getCurrentCover(MemoryStruct* cover)
{
   char* url = 0;

   // http://localhost:9000/music/current/cover.jpg?player=f0:4d:a2:33:b7:ed
   
   asprintf(&url, "http://%s:%d/music/current/cover.jpg?player=%s", 
            host, 9000, escId);

   downloadFile(url, cover);

   free(url);

   return success;
}
//***************************************************************************
// Get Cover
//***************************************************************************

int LmcCom::getCover(MemoryStruct* cover, int trackId)
{
   char* url = 0;

   // http://<server>:<port>/music/<track_id>/cover.jpg

   asprintf(&url, "http://%s:%d/music/%d/cover.jpg", 
            host, 9000, trackId);

   downloadFile(url, cover);

   free(url);

   return success;
}
