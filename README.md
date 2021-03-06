=====================
vdr-plugin-squeezebox
=====================

Copyright:            Jörg Wendel / horchi (vdr@jwendel.de)
Project's homepage:   https://github.com/horchi/vdr-plugin-squeezebox
Latest version at:    git clone https://github.com/horchi/vdr-plugin-squeezebox

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
See the file COPYING for more information.

The basic OSD drwaing function taken from skin-nopacity. Thanks to Louis Braun!

Description:
-------------

Plugin which act as a soft squeezebox


Requirements:
-------------

- libmagick++-dev
- running and reachable LMC in your network (see below)
- squeezelite (see below)
- a output device wich supports true color OSD

Installation:
-------------

1. Plugin

  #> git clone https://github.com/horchi/vdr-plugin-squeezebox
  #> cd vdr-plugin-squeezebox
  #> make
  #> make install

  may be you have to fix the access rights of <plugin-conf-path>/squeezebox


2. softhddevice Patch

  The squeezebox plugin need a deblocked sound device, a blacked video
  but avalible OSD output by the output device plugin. Exactly as I understand
  the play mode 'ePlayMode = pmAudioOnlyBlack' (audio only from player, no video (black screen)).

  I use the softhddevice plugin in version 0.6.1rc1 as output plugin, to enable the pmAudioOnlyBlack support in the softhddevice
  the patch softhddevice-pmAudioOnly.patch (deliverd with the squeezebox plugin) is needed!
  Therefore you habe to apply this patch and recompile softhddevice. Johns like to adopt the patch in one of the next versions.

  I don't know the behavior of pmAudioOnlyBlack relating to other output plugins like xine ...


3. LMS - Logitech Media Server

  Install the LMS on your server

    Logitech Version: http://www.mysqueezebox.com/download
    or the fork:      http://downloads.slimdevices.com/nightly/ (ich verwende aktuell die 7.8.0)

  Configure it via the WEB interface

4. squeezelite - streaming client for the LMS

  squeezelite is the the streming client used by the plugin
  Requirements: libasound2-dev libflac-dev libmad0-dev libvorbis-dev libfaad-dev libmpg123-dev

  #> git clone https://code.google.com/p/squeezelite
  #> cd squeezelite
  #> make
  #> cp squeezelite /usr/local/bin/squeezelite
  #> chmod 755 /usr/local/bin/squeezelite

Configuration:
--------------

  Put this in the setup.conf (until the plugins setup dialog is implemented):

   squeezebox.audioDevice = hw:CARD=NVidia,DEV=9
   squeezebox.lmcHost = localhost
   squeezebox.lmcHttpPort = 9000
   squeezebox.lmcPort = 9090
   squeezebox.logLevel = 1
   squeezebox.playerName = VDR-squeeze
   squeezebox.squeezeCmd = /usr/local/bin/squeezelite

  And adjust the parameters to fit your system:

  - squeezebox.audioDevice
     The possible setting of audioDevice are shown by calling:
     #> squeezelite -l
     normally similar to the softhddevice configuration (squeezelite option)

  - squeezebox.logLevel
     the loglevel (0-4), 0 less, 1 normal, 2.. debug (default 1)

  - squeezebox.playerName
     name of the squeezebox as displayed in the WEBIF (squeezelite option, default 'VDR-squeeze')

  - squeezebox.playerMac
     mac of the squeezebox, to run more than one squueze clinet in the same host (default the real mac)

  - squeezebox.squeezeCmd:
     path of squeezelite (default /usr/local/bin/squeezelite)

  - squeezebox.lmcHost
    host or ip of the LMS (default localhost)

  - squeezebox.lmcHttpPort
    http port of the LMS (default 9000)

  - squeezebox.lmcPort
    port of LMSs CLI interface (default 9090)

  - squeezebox.shadeTime
    inactivity time to shade the OSD for off (default 0)

  - squeezebox.shadeLevel
    shade level 0-100 in %, 100 is black (default 40)


Handling:
---------

Should intuitive ;)

- key 0 changes the color button setting
