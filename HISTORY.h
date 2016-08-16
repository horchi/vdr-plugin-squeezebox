/* 
 * -----------------------------------
 * squeezebox Plugin - Revision History
 * -----------------------------------
 *
 */

#define _VERSION     "0.0.14"
#define VERSION_DATE "16.08.2016"

#ifdef GIT_REV
#  define VERSION _VERSION "-GIT" GIT_REV
#else
#  define VERSION _VERSION
#endif

/*
 * ------------------------------------

2016-08-16: Version 0.0.14
  - bugfix: fixed crash on faild osd init
  
2014-02-22: Version 0.0.13
  - added: alsa options to configuration
  - bugfix: storage of log level (by alexander grothe)

2014-02-18: Version 0.0.12
  - added: delete of player instance on plugin exit

2014-02-17: Version 0.0.11
  - added  Setup menu
  - added  auto scroll for lyrics 
  - added  shadeLevel (0-100) to config

2014-02-02: Version 0.0.10
  - change  fixed spacing below cover box
  - change  added lyrics 
  - bugfix  waiting on player startup

2014-01-30: Version 0.0.9
  - added   station play time for webradio instead of track time/duration
  - added   'favorites' to menu
  - added   'new music' to menu
  - bugfix  fixed missing PNG error
  - added   support of VDRs 'MenuScrollWrap' setting
  - added   shade on inacivity (use README for details)
  - added   start replay on plugin start
  - added   dimmed cover as menu background

2014-01-28: Version 0.0.8
  - added   key repead
  - added   support of long artist, genre, ... lists
  - added   cover art for web radio
  - change  cover load without fs usage (speed)
  - added   track info for webradio (station name, ...)
  - change  improved scroll speed of playlist
  - added   menue for web radio
  - added   TODO list
  - added   mac to configuration (setup.conf)
  - added   artwork cache to speedup redraw

2014-01-25: Version 0.0.7
  - added   own menu instead of skins-menu

2014-01-24: Version 0.0.6
  - added   menu structure like LMCs WEBIF
  - change  PNGs stored in resdir (instead of cfgdir)
  - added   DEST target to make install

2014-01-20: Version 0.0.5
  - added   osd redraw on vdr volume dispay

2014-01-20: Version 0.0.4
  - changed  color button radius
  - change  view of progress bar even replay is paused

2014-01-20: Version 0.0.3
  - First release

2014-01-12: Version 0.0.2
  - First working beta.

2014-01-02: Version 0.0.1
  - Initial revision.

 * ------------------------------------
 */
