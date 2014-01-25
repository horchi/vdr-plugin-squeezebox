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
      int AddSubMenu(cMenuBase* subMenu) { activeMenu->setParent(this); activeMenu->setVisibleItems(visibleItems); return done; };
      virtual int ProcessKey(int key);

      void setParent(cMenuBase* m)       { parent = m; }
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

   private:

      static cMenuBase* activeMenu;

      int current;
      int visibleItems;
      cMenuBase* parent;
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

      cSubMenu(const char* title, LmcCom* aLmc, LmcTag::Tag tag, LmcCom::Parameters* aFilters = 0);
      virtual ~cSubMenu();
      virtual int ProcessKey(int key);

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
