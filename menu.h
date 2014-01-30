/*
 * menu.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "lmctag.h"

//***************************************************************************
// Menu Item
//***************************************************************************

class cMenuItem : public cListObject
{
   public:

      cMenuItem(const char* aText) { text = strdup(aText); }
      ~cMenuItem()                 { free(text); }

      const char* getText()        { return text; }

   private:
      
      char* text;
};

//***************************************************************************
// Sub Menu Item
//***************************************************************************

class cSubMenuItem : public cMenuItem
{
   public:
   
      cSubMenuItem(LmcCom::ListItem* aItem) : cMenuItem(aItem->content.c_str()) 
      { item = *aItem; }

      virtual ~cSubMenuItem()
      { }

      const char* getText()              { return item.content.c_str(); }
      const LmcCom::ListItem* getItem()  { return &item; }

   protected:

      LmcCom::ListItem item;
};

//***************************************************************************
// Menu Base
//***************************************************************************

class cMenuBase : public cList<cMenuItem>
{
   public:

      cMenuBase(const char* aTitle);
      ~cMenuBase();

      const char* Title()                { return title; }
      int AddSubMenu(cMenuBase* subMenu) {  activeMenu->setVisibleItems(visibleItems); return done; };
      virtual int ProcessKey(int key);

      static cMenuBase* getActive()      { return activeMenu; }

      int getCount()                     { return Count(); }
      int getCurrent()                   { return current; }
      const char* getItemTextAt(int i)   { return Get(i)->getText(); }

      void setHelp(const char* r, const char* g, const char* y, const char* b);
      void setVisibleItems(int n) { visibleItems = n; }

      const char* Red()     { return red ? red : ""; }
      const char* Green()   { return green ? green : ""; }
      const char* Yellow()  { return yellow ? yellow : ""; }
      const char* Blue()    { return blue ? blue : ""; }

   protected:

      cMenuBase* parent;

   private:

      static cMenuBase* activeMenu;

      int current;
      int visibleItems;
      char* title;
      char* red;
      char* green;
      char* yellow;
      char* blue;
};

//***************************************************************************
// Menu Squeeze
//***************************************************************************

class cMenuSqueeze : public cMenuBase
{
   public:

      cMenuSqueeze(const char* aTitle, LmcCom* aLmc); 
      
      virtual int ProcessKey(int key);

   private:

      LmcCom* lmc;
};

//***************************************************************************
// Sub Menu
//***************************************************************************

class cSubMenu : public cMenuSqueeze
{
   public:

      cSubMenu(cMenuBase* aParent, const char* title, LmcCom* aLmc, 
               LmcCom::RangeQueryType aQueryType, LmcCom::Parameters* aFilters = 0);
      virtual ~cSubMenu();
      virtual int ProcessKey(int key);

   protected:

      struct Query
      {
         LmcCom::RangeQueryType queryType;
         LmcTag::Tag idtag;                      // id tag like genre_id (used to filter)
         LmcCom::RangeQueryType subLevelQuery;   // next deeper menu level
      };
      
   private:

      LmcCom::Parameters filters;
      LmcCom* lmc;
      LmcCom::RangeQueryType queryType;

      // static stuff

      static Query queries[];

      static LmcTag::Tag toIdTag(LmcCom::RangeQueryType queryType)
      {
         for (int i = 0; queries[i].queryType != LmcCom::rqtUnknown; i++)
            if (queries[i].queryType == queryType)
               return queries[i].idtag;
         
         return LmcTag::tUnknown;
      }

      static LmcCom::RangeQueryType toSubLevelQuery(LmcCom::RangeQueryType queryType)
      {
         for (int i = 0; queries[i].queryType != LmcCom::rqtUnknown; i++)
            if (queries[i].queryType == queryType)
               return queries[i].subLevelQuery;
         
         return LmcCom::rqtUnknown;
      }
};
