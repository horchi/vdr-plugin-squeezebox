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
   red = 0;
   green = 0;
   yellow = 0;
   blue = 0;
   activeMenu = this;
   visibleItems = 0;
   parent = 0;

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

//***************************************************************************
// Process Key
//***************************************************************************

int cMenuBase::ProcessKey(int key)
{
   switch (key)
   {
      case kUp|k_Repeat:
      case kUp:
      {
         if (current > 0)         
            current--; 
         else if (Setup.MenuScrollWrap)
            current = Count()-1;

         return done;        
      }

      case kDown|k_Repeat:
      case kDown: 
      {
         if (current < Count()-1) 
            current++; 
         else if (Setup.MenuScrollWrap)
            current = 0;

         return done;
      }
         
      case kLeft|k_Repeat:
      case kLeft:
      {
         if (current > 0)
            current = max(current-visibleItems, 0);
         else if (Setup.MenuScrollWrap)
            current = Count()-1;

         return done;
      }

      case kRight|k_Repeat:
      case kRight:
      {
         if (current < Count()-1)
            current = min(current+visibleItems, Count()-1);
         else if (Setup.MenuScrollWrap)
            current = 0;

         return done;
      }

      case kRed:  return end;
      case kBack: return back;
      case k0:    return done;
   }

   return ignore;
}

//***************************************************************************
// Definition of the Menu Structure
//***************************************************************************

cSubMenu::Query cSubMenu::queries[] =
{
   { LmcCom::rqtPlaylists, LmcTag::tPlaylistId, LmcCom::rqtUnknown    },
   { LmcCom::rqtGenres,    LmcTag::tGenreId,    LmcCom::rqtArtists    },
   { LmcCom::rqtArtists,   LmcTag::tArtistId,   LmcCom::rqtAlbums     },
   { LmcCom::rqtAlbums,    LmcTag::tAlbumId,    LmcCom::rqtTracks     },
   { LmcCom::rqtNewMusic,  LmcTag::tAlbumId,    LmcCom::rqtUnknown    },
   { LmcCom::rqtYears,     LmcTag::tYear,       LmcCom::rqtAlbums     },
   { LmcCom::rqtTracks,    LmcTag::tTrackId,    LmcCom::rqtUnknown    },
   { LmcCom::rqtRadios,    LmcTag::tName,       LmcCom::rqtRadioApps  },
   { LmcCom::rqtRadioApps, LmcTag::tItemId,     LmcCom::rqtRadioApps  },
   { LmcCom::rqtFavorites, LmcTag::tItemId,     LmcCom::rqtUnknown    },

   { LmcCom::rqtUnknown }
};

//***************************************************************************
// Menu
//***************************************************************************

cSubMenu::cSubMenu(cMenuBase* aParent, const char* title, LmcCom* aLmc, 
                   LmcCom::RangeQueryType aQueryType, LmcCom::Parameters* aFilters)
   : cMenuSqueeze(title, aLmc)
{
   static int maxElements = 50000;
   LmcCom::RangeList list;
   int total;

   parent = aParent;
   lmc = aLmc;
   queryType = aQueryType;

   Clear();

   if (queryType == LmcCom::rqtRadioApps)
   {
      cSubMenuItem* item = (cSubMenuItem*)parent->Get(parent->getCurrent());   

      if (parent)
      {
         char flt[500+TB];

         filters.clear();

         if (toIdTag(queryType) != LmcTag::tUnknown)   // tIsAudio !!!
         {
            snprintf(flt, 500, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
            filters.push_back(flt);
         }

         if (item && item->getItem())
         {
            tell(eloDebug, "Radio command: '%s' with '%s'", item->getItem()->command.c_str(), flt);
            
            if (lmc && lmc->queryRange(queryType, 0, maxElements, &list, total, item->getItem()->command.c_str(), &filters) == success)
            {
               LmcCom::RangeList::iterator it;
               
               for (it = list.begin(); it != list.end(); ++it)
               {
                  if ((*it).command == "search")    // not implemented
                     continue;

                  if ((*it).command.empty())
                     (*it).command = item->getItem()->command;

                  Add(new cSubMenuItem(&(*it)));
               }
            }
         }
      }
   }
   else
   {
      if (aFilters)
         filters = *aFilters;

      if (queryType == LmcCom::rqtNewMusic)
         filters.push_back("sort:new");

      if (lmc && lmc->queryRange(queryType, 0, maxElements, &list, total, "", &filters) == success)
      {
         LmcCom::RangeList::iterator it;
         
         for (it = list.begin(); it != list.end(); ++it)
            Add(new cSubMenuItem(&(*it)));
         
         if (total > maxElements)
            tell(eloAlways, "Warning: %d more, only maxElements supported", total-maxElements);
      }
   }

   // #TODO, change help info with current item while scrolling

   cSubMenuItem* item = (cSubMenuItem*)Get(0);

   if (item && item->getItem()->isAudio)
      setHelp(tr("Close"), tr("Insert"), tr("Append"), tr("Play"));
   else
      setHelp(tr("Close"), 0, 0, 0);
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

   if (key == kOk)
   {
      if (toSubLevelQuery(queryType) != LmcCom::rqtUnknown)
      {
         char* subTitle;
         LmcCom::Parameters pars = filters;
         int addSub = queryType == LmcCom::rqtRadioApps ? item->getItem()->hasItems : yes;

         if (addSub)
         {
            asprintf(&subTitle, "%s / %s ", Title(), item->getItem()->content.c_str());
            sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), 
                    queryType == LmcCom::rqtYears ? item->getItem()->content.c_str() : item->getItem()->id.c_str());
            pars.push_back(flt);
            
            AddSubMenu(new cSubMenu(this, subTitle, lmc, toSubLevelQuery(queryType), &pars));
            free(subTitle);
         }
      }
      
      return done;
   }

   else if (item->getItem()->isAudio)
   {
      LmcCom::Parameters pars;

      switch (key)
      {
         case kGreen:
         {
            if (queryType < LmcCom::rqtRadios)
            {
               pars = filters;
               pars.push_back("cmd:insert");
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               lmc->execute("playlistcontrol", &pars);
            }
            else
            {
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               sprintf(flt, "%s playlist insert", item->getItem()->command.c_str());
               lmc->execute(flt, &pars);
            }
            
            return done;
         }

         case kYellow:
         {
            if (queryType < LmcCom::rqtRadios)
            {
               pars = filters;            
               pars.push_back("cmd:add");
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               lmc->execute("playlistcontrol", &pars);
            }
            else
            {
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               sprintf(flt, "%s playlist add", item->getItem()->command.c_str());
               lmc->execute(flt, &pars);
            }
            
            return done;
         }
  

         case kBlue:         
         {     
            if (queryType < LmcCom::rqtRadios)
            {
               pars = filters;
               pars.push_back("cmd:load");
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               lmc->execute("playlistcontrol", &pars);
            }
            else
            {
               sprintf(flt, "%s:%s", LmcTag::toName(toIdTag(queryType)), item->getItem()->id.c_str());
               pars.push_back(flt);
               sprintf(flt, "%s playlist play", item->getItem()->command.c_str());
               lmc->execute(flt, &pars);
            }
            
            return done;
         }
         
         default: 
            return ignore;
      }
   }
   else
   {
      if (key == kGreen || key == kYellow || key == kBlue)
         return done;
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

   Add(new cMenuItem(tr("Artists")));
   Add(new cMenuItem(tr("Albums")));
   Add(new cMenuItem(tr("Genres")));
   Add(new cMenuItem(tr("Years")));
   Add(new cMenuItem(tr("Play random tracks")));
   Add(new cMenuItem(tr("Playlists")));
   Add(new cMenuItem(tr("Radio")));
   Add(new cMenuItem(tr("Favorites")));
   Add(new cMenuItem(tr("New Music")));

   setHelp(tr("Close"), 0, 0, 0);
}

//***************************************************************************
// Process Key
//***************************************************************************

int cMenuSqueeze::ProcessKey(int key)
{
   int state;

   if ((state = cMenuBase::ProcessKey(key)) != ignore)
      return state;

   cMenuItem* item = (cMenuItem*)Get(getCurrent());

   switch (key)
   {
      case kBlue: 
      case kGreen:
      case kYellow: return done;

      case kOk:
      {
         switch (getCurrent())
         {
            case 0: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtArtists));
            case 1: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtAlbums));
            case 2: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtGenres));
            case 3: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtYears));
            case 4: lmc->randomTracks(); return done;
            case 5: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtPlaylists));
            case 6: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtRadios));
            case 7: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtFavorites));
            case 8: return AddSubMenu(new cSubMenu(this, item->getText(), lmc, LmcCom::rqtNewMusic));
         }
      }

      default: 
         break;
   }

   return ignore;
}
