/*
 * menu.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "squeezebox.h"
#include "menu.h"

//***************************************************************************
// Manu Base
//***************************************************************************

cMenuBase* cMenuBase::activeMenu = 0;

cMenuBase::cMenuBase(const char* aTitle)      
{ 
   title = strdup(aTitle); 
   current = 0;
   parent = 0;
   red = 0;
   green = 0;
   yellow = 0;
   blue = 0;
   activeMenu = this;
   visibleItems = 0;

   Clear();
}

cMenuBase::~cMenuBase()
{ 
   activeMenu = parent; 
   free(title); 
   free(red);
   free(green);
   free(yellow);
   free(blue); 
}

void cMenuBase::setHelp(const char* r, const char* g, const char* y, const char* b)
{
   free(red);    red    = r ? strdup(r) : 0;
   free(green);  green  = g ? strdup(g) : 0;
   free(yellow); yellow = y ? strdup(y) : 0;
   free(blue);   blue   = b ? strdup(b) : 0;
}

int cMenuBase::ProcessKey(int key)
{
   switch (key)
   {
      case kUp:   if (current > 0)         current--; return done;
      case kDown: if (current < Count()-1) current++; return done;
         
      case kLeft:
      {
         if (current > 0)
            current = max(current-visibleItems, 0);

         return done;
      }
      case kRight:
      {
         if (current < Count()-1)
            current = min(current+visibleItems, Count()-1);

         return done;
      }

      case kBack: return end;
   }

   return ignore;
}

//***************************************************************************
// Definition of the Menu Structure
//***************************************************************************

cSubMenu::Query cSubMenu::queries[] =
{
   { LmcTag::tPlaylist, LmcTag::tPlaylistId, LmcTag::tUnknown,    "playlists" },
   { LmcTag::tGenre,    LmcTag::tGenreId,    LmcTag::tArtist,     "genres"    },
   { LmcTag::tArtist,   LmcTag::tArtistId,   LmcTag::tAlbum,      "artists"   },
   { LmcTag::tAlbum,    LmcTag::tAlbumId,    LmcTag::tTrack,      "albums"    },
   { LmcTag::tYear,     LmcTag::tYear,       LmcTag::tAlbum,      "years"     },
   { LmcTag::tTrack,    LmcTag::tTrackId,    LmcTag::tUnknown,    "tracks"    },
   
   { LmcTag::tUnknown }
};

//***************************************************************************
// Menu
//***************************************************************************

cSubMenu::cSubMenu(const char* title, LmcCom* aLmc, LmcTag::Tag aTag, LmcCom::Parameters* aFilters)
   : cMenuSqueeze(title, aLmc)
{
   LmcCom::RangeList list;
   int total;
   
   lmc = aLmc;
   tag = aTag;
   
   if (aFilters)
      filters = *aFilters;

   Clear();

   if (lmc && lmc->queryRange(toName(tag), tag, 0, 200, &list, total, &filters) == success)
   {
      LmcCom::RangeList::iterator it;
      
      for (it = list.begin(); it != list.end(); ++it)
         Add(new cSubMenuItem(&(*it)));
      
      if (total > 200)
         tell(eloAlways, "Warning: [%s] %d more, only 200 supported", toName(tag), total-200);
   }

   setHelp(tr("insert"), tr("play"), tr("append"), 0);

   // Display();
}

cSubMenu::~cSubMenu() 
{ 
}

//***************************************************************************
// Process Key
//***************************************************************************

int cSubMenu::ProcessKey(int key)
{
   int state;
   char flt[500];

   if ((state = cMenuBase::ProcessKey(key)) != ignore)
      return state;

   cSubMenuItem* item = (cSubMenuItem*)Get(getCurrent());

   switch (key)
   {
      case kOk:
      {
         if (toSubLevelTag(tag) != LmcTag::tUnknown)
         {
            char* subTitle;
            LmcCom::Parameters pars = filters;
            
            asprintf(&subTitle, "%s / %s ", Title(), item->getItem()->content.c_str());
            sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), tag == LmcTag::tYear ? atoi(item->getItem()->content.c_str()) : item->getItem()->id);
            pars.push_back(flt);
            
            AddSubMenu(new cSubMenu(subTitle, lmc, toSubLevelTag(tag), &pars));
            free(subTitle);
         }
         
         return done;
      }

      case kRed:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:insert");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return done;
      }

      case kGreen:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:load");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return done;
      }
      
      case kYellow:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:add");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return done;
      }

      case kBlue: return done;

      default: 
         return ignore;
   }

   return ignore;
}

//***************************************************************************
// Menu
//***************************************************************************

cMenuSqueeze::cMenuSqueeze(const char* aTitle, LmcCom* aLmc)
   : cMenuBase(aTitle)
{
   lmc = aLmc;

   Add(new cOsdItem(tr("Play random tracks")));
   Add(new cOsdItem(tr("Playlists")));
   Add(new cOsdItem(tr("Genres")));
   Add(new cOsdItem(tr("Artists")));
   Add(new cOsdItem(tr("Albums")));
   Add(new cOsdItem(tr("Years")));

   setHelp(0, 0, 0, 0);

   // Display();
}

//***************************************************************************
// Process Key
//***************************************************************************

int cMenuSqueeze::ProcessKey(int key)
{
   int state;

   if ((state = cMenuBase::ProcessKey(key)) != ignore)
      return state;

   switch (key)
   {
      case kRed:
      case kBlue: 
      case kGreen:
      case kYellow: return done;

      case kOk:
      {
         switch (getCurrent())
         {
            case 0: lmc->randomTracks(); return done;
            case 1: return AddSubMenu(new cSubMenu(tr("Playlists"), lmc, LmcTag::tPlaylist));
            case 2: return AddSubMenu(new cSubMenu(tr("Genres"), lmc, LmcTag::tGenre));
            case 3: return AddSubMenu(new cSubMenu(tr("Artists"), lmc, LmcTag::tArtist));
            case 4: return AddSubMenu(new cSubMenu(tr("Albums"), lmc, LmcTag::tAlbum));
            case 5: return AddSubMenu(new cSubMenu(tr("Years"), lmc, LmcTag::tYear));
         }
      }

      default: 
         break;
   }

   return ignore;
}
