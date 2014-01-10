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
}

//***************************************************************************
// Open
//***************************************************************************

int LmcCom::open(const char* aHost, unsigned short aPort)
{
   free(host);
   port = aPort;
   host = strdup(aHost);

   return TcpChannel::open(port, host);
}

//***************************************************************************
// Update Currernt Track
//***************************************************************************

LmcCom::TrackInfo* LmcCom::updateCurrerntTrack()
{
   int status;

   status = query("genre",   currentTrack.genre,    sizeof(currentTrack.genre))
      + query("album",       currentTrack.album,    sizeof(currentTrack.album))
      + query("artist",      currentTrack.artist,   sizeof(currentTrack.artist))
      + query("title",       currentTrack.title,    sizeof(currentTrack.title))
      + queryInt("duration", currentTrack.duration)
      + queryInt("time",     currentTrack.time)
      + query("path",        currentTrack.path,     sizeof(currentTrack.path));

   if (status != success)
      return 0;

   currentTrack.updatedAt = cTimeMs::Now();
   currentTrack.remaining = currentTrack.duration - currentTrack.time;
   unescape(currentTrack.path);   // path is double escaped :o
   
   return &currentTrack;
}

//***************************************************************************
// Update Player State
//***************************************************************************

LmcCom::PlayerInfo* LmcCom::updatePlayerState()
{
   int status;

   status = query("mode",              playerState.mode,     sizeof(playerState.mode))
      + query("version",               playerState.version,  sizeof(playerState.version))
      + queryInt("mixer volume",       playerState.volume)
      + queryInt("mixer muting",       playerState.muted)
      + queryInt("playlist index",     playerState.plIndex)
      + queryInt("playlist tracks",    playerState.plCount)
      + queryInt("playlist shuffle",   playerState.plShuffle)
      + queryInt("playlist repeat",   playerState.plRepeat)
      + query("player id VDR-Squeeze", playerState.id,       sizeof(playerState.id));

   if (status != success)
   {
      tell(eloAlways, "Error: Requesting player state failed");
      return 0;
   }

   return &playerState;
}

//***************************************************************************
// Query
//***************************************************************************

int LmcCom::query(const char* command, char* result, int max)
{
   int status;

   request(command);
   write(" ?\n");

   if ((status = response(result, max)) != success)
       tell(0, "Error: Request of '%s' failed", command);
      
   return status;
}

int LmcCom::queryInt(const char* command, int& value)
{
   int status = success;

   char result[100+TB];

   status = request(command);
   status += write(" ?\n");
   status += response(result, 100);

   if (status == success)
      value = atol(result);

   return status;
}

//***************************************************************************
// Execute
//***************************************************************************

int LmcCom::execute(const char* command)
{
   request(command);
   write("\n");

   return response();
}

//***************************************************************************
// Request
//***************************************************************************

int LmcCom::request(const char* command)
{
   int status;

   if (strlen(command) > sizeMaxCommand)
      return fail;

   strcpy(lastCommand, command);

   status = write(escId)
      + write(" ")
      + write(lastCommand);

   return status;
}

//***************************************************************************
// Response
//***************************************************************************

int LmcCom::response(char* result, int max)
{
   char buf[1000+TB];

   if (look(1) == success)
   {
      if (read(buf, 1000, yes) == success)
      {
         const char* p;

         buf[strlen(buf)-1] = 0;   // cut LF

         unescape(buf);
         
         if ((p = strstr(buf, lastCommand)))
         {
            p += strlen(lastCommand)+1;

            if (result && max)
               sprintf(result, "%.*s", max, p);

            tell(eloDebug, "<- (response) [%s]", p);

            return success;
         }

         tell(0, "Got unexpected answer [%s]", buf);
      }

      return fail;
   }

   return fail;  
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

char* LmcCom::escape(char* buf)
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

int LmcCom::checkNotify()
{
   int status  = fail;
   char buf[1000+TB];

   if (!notify)
   {
      tell(0, "Cant check for notifications until startNotify is called");
      return fail;
   }

   if (notify->look(1) == success)
   {
      if (notify->read(buf, 1000, yes) == success)
      {
         const char* p;

         buf[strlen(buf)-1] = 0;   // cut LF

         unescape(buf);
         
         if ((p = strstr(buf, "ed ")))
         {
            p += strlen("ed ");
            status = success;

            tell(eloDebug, "<- [%s]", p);

            if (strstr(p, "newsong"))
            {
               updateCurrerntTrack();
               status = ifoTrack;
            }

            else if (strstr(p, "pause") || strstr(p, "server"))
            {
               updatePlayerState();
               updateCurrerntTrack();
               status = ifoPlayer;
            }

            return status;
         }
      }

      return fail;
   }

   return wrnNoEventPending;
}

//***************************************************************************
// Get Current Cover
//***************************************************************************

int LmcCom::getCurrentCover(MemoryStruct* cover)
{
   int res;
   char* url = 0;

   // http://localhost:9000/music/current/cover.jpg?player=f0:4d:a2:33:b7:ed
   
   res = asprintf(&url, "http://%s:%d/music/current/cover.jpg?player=%s", 
            host, 9000, escId);

   downloadFile(url, cover);

   free(url);

   return success;
}
