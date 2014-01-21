/*
 * menu.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "squeezebox.h"

enum FilterType
{
   ftGenre          = 1,
   ftArtist         = 2,
   ftAlbum          = 4,
   ftYear           = 8,
   
   ftCombined       = 16
   ftGenreAlbum     = ftCombined,

   ftOther          = 256,
   ftPlaylist       = ftOther,
   ftRandomTracks   = 512 
};

//***************************************************************************
// Sub Menu Item
//***************************************************************************

class cSubMenuItem : public cOsdItem
{
   public:
   
      cSubMenuItem(const char* title) : cOsdItem(title) 
      { value = strdup(title); }

      virtual ~cSubMenuItem()      { free(value); }

      const char* getValue()       { return value; }

   protected:

      char* value;
};

//***************************************************************************
// Sub Menu
//***************************************************************************

class cSubMenu : public cOsdMenu
{
   public:

      cSubMenu(const char* title, LmcCom* aLmc, FilterType tp, const char* aQuery);
      virtual ~cSubMenu() { free(query); }
      
      virtual eOSState ProcessKey(eKeys key);

   protected:

      LmcCom::RangeList list;
      char* query;
      LmcCom* lmc;
      FilterType type;
};

cSubMenu::cSubMenu(const char* title, LmcCom* aLmc, FilterType tp, const char* aQuery)
   : cOsdMenu(title)
{
   int total;

   SetMenuCategory(mcMain);
   lmc = aLmc;
   type = tp, 
   query = strdup(aQuery);

   Clear();

   if (type < ftCombined)
   {
      if (lmc && lmc->queryRange(query, 0, 200, &list, total) == success)
      {
         LmcCom::RangeList::iterator it;
         
         for (it = list.begin(); it != list.end(); ++it)
            cOsdMenu::Add(new cSubMenuItem((*it).c_str()));
         
         if (total > 200)
            tell(eloAlways, "Warning: [%s] %d more, only 200 supported", query, total-200);
      }
   }
   else
   {
      
   }

   SetHelp(0, tr("play"), tr("append"), 0);

   Display();
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSubMenu::ProcessKey(eKeys key)
{
   eOSState state = cOsdMenu::ProcessKey(key);

   if (state != osUnknown)
      return state;

   switch (key)
   {
      case kGreen:
      {
         cSubMenuItem* item = (cSubMenuItem*)Get(Current());

         switch (type)
         {
            case ftGenre:    lmc->loadAlbum(item->getValue(), "*", "*"); break;
            case ftArtist:   lmc->loadAlbum("*", item->getValue(), "*"); break;
            case ftAlbum:    lmc->loadAlbum("*", "*", item->getValue()); break;
            case ftYear:     break;
            case ftPlaylist: lmc->loadPlaylist(item->getValue());        break;
            default: break;
         }

         break;
      }   
      case kYellow:
      {
         cSubMenuItem* item = (cSubMenuItem*)Get(Current());

         switch (type)
         {
            case ftGenre:    lmc->appendAlbum(item->getValue(), "*", "*"); break;
            case ftArtist:   lmc->appendAlbum("*", item->getValue(), "*"); break;
            case ftAlbum:    lmc->appendAlbum("*", "*", item->getValue()); break;
            case ftYear:     break;
            case ftPlaylist: lmc->appendPlaylist(item->getValue());        break;
            default: break;
         }

         break;
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
//   cOsdMenu::Add(new cOsdItem(tr("Years")));

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

            case 1: return AddSubMenu(new cSubMenu(tr("Playlists"), lmc, ftPlaylist, "playlists"));
            case 2: return AddSubMenu(new cSubMenu(tr("Genres"), lmc, ftGenre, "genres"));
            case 3: return AddSubMenu(new cSubMenu(tr("Artists"), lmc, ftArtist, "artists"));
            case 4: return AddSubMenu(new cSubMenu(tr("Albums"), lmc, ftAlbum, "albums"));
            case 5: return AddSubMenu(new cSubMenu(tr("Years"), lmc, ftYear, "years"));
         }
      }

      default:
         break;
   }

   return state;
}
