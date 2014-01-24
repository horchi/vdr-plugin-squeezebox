/*
 * menu.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "squeezebox.h"
#include "lmctag.h"

//***************************************************************************
// Sub Menu Item
//***************************************************************************

class cSubMenuItem : public cOsdItem
{
   public:
   
      cSubMenuItem(LmcCom::ListItem* aItem) : cOsdItem(aItem->content.c_str()) 
      { item = *aItem; }

      virtual ~cSubMenuItem()
      { }

      const LmcCom::ListItem* getItem()       { return &item; }

   protected:

      LmcCom::ListItem item;
};

//***************************************************************************
// Sub Menu
//***************************************************************************

class cSubMenu : public cOsdMenu
{
   public:

      cSubMenu(const char* title, LmcCom* aLmc, LmcTag::Tag tag, LmcCom::Parameters* aFilters = 0);
      virtual ~cSubMenu();
      virtual eOSState ProcessKey(eKeys key);

   protected:

      struct Query
      {
         LmcTag::Tag tag;         // tag like genre (used for request)
         LmcTag::Tag idtag;       // id tag like genre_id (used to filter)
         LmcTag::Tag tagSubLevel; // next deeper menu level
         const char* query;
      };
      
   private:

      LmcCom::Parameters filters;
      LmcCom* lmc;
      LmcTag::Tag tag;

      // static stuff

      static Query queries[];

      static const char* toName(LmcTag::Tag tag)
      {
         for (int i = 0; queries[i].tag != LmcTag::tUnknown; i++)
            if (queries[i].tag == tag)
               return queries[i].query;
         
         return "";
      }

      static LmcTag::Tag toIdTag(LmcTag::Tag tag)
      {
         for (int i = 0; queries[i].tag != LmcTag::tUnknown; i++)
            if (queries[i].tag == tag)
               return queries[i].idtag;
         
         return LmcTag::tUnknown;
      }

      static LmcTag::Tag toSubLevelTag(LmcTag::Tag tag)
      {
         for (int i = 0; queries[i].tag != LmcTag::tUnknown; i++)
            if (queries[i].tag == tag)
               return queries[i].tagSubLevel;
         
         return LmcTag::tUnknown;
      }
};

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
   : cOsdMenu(title)
{
   LmcCom::RangeList list;
   int total;
   
   SetMenuCategory(mcMain);
   lmc = aLmc;
   tag = aTag;
   
   if (aFilters)
      filters = *aFilters;

   Clear();

   if (lmc && lmc->queryRange(toName(tag), tag, 0, 200, &list, total, &filters) == success)
   {
      LmcCom::RangeList::iterator it;
      
      for (it = list.begin(); it != list.end(); ++it)
         cOsdMenu::Add(new cSubMenuItem(&(*it)));
      
      if (total > 200)
         tell(eloAlways, "Warning: [%s] %d more, only 200 supported", toName(tag), total-200);
   }

   SetHelp(tr("insert"), tr("play"), tr("append"), 0);

   Display();
}

cSubMenu::~cSubMenu() 
{ 
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSubMenu::ProcessKey(eKeys key)
{
   char flt[500];
   eOSState state = cOsdMenu::ProcessKey(key);
   cSubMenuItem* item = (cSubMenuItem*)Get(Current());

   if (state != osUnknown)
      return state;

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
            
            state = AddSubMenu(new cSubMenu(subTitle, lmc, toSubLevelTag(tag), &pars));
            free(subTitle);
            return state;
         }
         
         return osContinue;
      }

      case kRed:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:insert");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return osContinue;
      }

      case kGreen:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:load");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return osContinue;
      }
      
      case kYellow:
      {
         LmcCom::Parameters pars = filters;

         pars.push_back("cmd:add");
         sprintf(flt, "%s:%d", LmcTag::toName(toIdTag(tag)), item->getItem()->id);
         pars.push_back(flt);
         lmc->execute("playlistcontrol", &pars);

         return osContinue;
      }

      default: return state;
   }

   return state;
}

//***************************************************************************
// Main Menu
//***************************************************************************

cSqueezeMenu::cSqueezeMenu(const char* title, LmcCom* aLmc)
   : cOsdMenu(title)
{
   lmc = aLmc;

   SetMenuCategory(mcMain);
   Clear();

   cOsdMenu::Add(new cOsdItem(tr("Play random tracks")));
   cOsdMenu::Add(new cOsdItem(tr("Playlists")));
   cOsdMenu::Add(new cOsdItem(tr("Genres")));
   cOsdMenu::Add(new cOsdItem(tr("Artists")));
   cOsdMenu::Add(new cOsdItem(tr("Albums")));
   cOsdMenu::Add(new cOsdItem(tr("Years")));

   SetHelp(0, 0, 0, 0);

   Display();
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSqueezeMenu::ProcessKey(eKeys key)
{
   eOSState state = cOsdMenu::ProcessKey(key);

   if (state != osUnknown)
      return state;

   switch (key)
   {
      case kOk:
      {
         switch (Current())
         {
            case 0: lmc->randomTracks(); return osEnd;
            case 1: return AddSubMenu(new cSubMenu(tr("Playlists"), lmc, LmcTag::tPlaylist));
            case 2: return AddSubMenu(new cSubMenu(tr("Genres"), lmc, LmcTag::tGenre));
            case 3: return AddSubMenu(new cSubMenu(tr("Artists"), lmc, LmcTag::tArtist));
            case 4: return AddSubMenu(new cSubMenu(tr("Albums"), lmc, LmcTag::tAlbum));
            case 5: return AddSubMenu(new cSubMenu(tr("Years"), lmc, LmcTag::tYear));
         }
      }

      default: break;
   }

   return state;
}
