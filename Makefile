#
# Makefile for a Video Disk Recorder plugin
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN   = squeezebox
HISTFILE = "HISTORY.h"
DEBUG    = 1

# External image lib to use: imagemagick, graphicsmagick
IMAGELIB = imagemagick

### The version number of this plugin (taken from the main source file):

#VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).h | awk '{ print $$6 }' | sed -e 's/[";]//g')
VERSION = $(shell grep 'define _VERSION ' $(HISTFILE) | awk '{ print $$3 }' | sed -e 's/[";]//g')

LASTHIST    = $(shell grep '^20[0-3][0-9]' $(HISTFILE) | head -1)
LASTCOMMENT = $(subst |,\n,$(shell sed -n '/$(LASTHIST)/,/^ *$$/p' $(HISTFILE) | tr '\n' '|'))
LASTTAG     = $(shell git describe --tags --abbrev=0)
BRANCH      = $(shell git rev-parse --abbrev-ref HEAD)
GIT_REV = $(shell git describe --always 2>/dev/null)

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell pkg-config --variable=$(1) vdr || pkg-config --variable=$(1) ../../../vdr.pc))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)

CXXFLAGS += -Wno-unused-result -Wreturn-type -Wno-long-long \
            -Wformat -Wunused-variable -Wunused-label \
            -pedantic -Wunused-value -Wunused-function \

ifdef DEBUG
  CXXFLAGS += -ggdb -O0
  CFLAGS += -ggdb -O0
endif

### The plugin config/resource directory

export CFGDIR = $(call PKGCFG,configdir)/plugins/$(PLUGIN)/
export RESDIR = $(call PKGCFG,resdir)/plugins/$(PLUGIN)/

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

LIBS = -lcurl

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

ifeq ($(IMAGELIB), imagemagick)
	INCLUDES += $(shell pkg-config --cflags Magick++)
	LIBS += $(shell pkg-config --libs Magick++)
else ifeq ($(IMAGELIB), graphicsmagick)
	INCLUDES += $(shell pkg-config --cflags GraphicsMagick++)
	LIBS += $(shell pkg-config --libs GraphicsMagick++)
endif

DEFINES += -DVDR_PLUGIN -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = $(PLUGIN).o lmccom.o osd2web.o osd.o menu.o config.o player.o helpers.o \
     lmctag.o imgtools.o lib/common.o lib/tcpchannel.o lib/curl.o

ifdef GIT_REV
   DEFINES += -DGIT_REV='"$(GIT_REV)"'
endif

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install-res:
	mkdir -p "$(DESTDIR)$(RESDIR)"
	chmod 755 "$(DESTDIR)$(RESDIR)"
	install --mode=644 -D ./config/* $(DESTDIR)$(RESDIR)

install: install-lib install-i18n install-res

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) --exclude debian --exclude CVS --exclude .svn --exclude .hg --exclude .git $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot $(PODIR)/*~
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~ lib/*~ lib/*.o tt

tt: test.c lmccom.c lib/tcpchannel.c lib/common.c
	$(CXX) $(CXXFLAGS) test.c lmctag.c lmccom.c lib/tcpchannel.c lib/common.c lib/curl.c $(LIBS) -o tt

cppchk:
	cppcheck --template="{file}:{line}:{severity}:{message}" --quiet --force *.c *.h

# ------------------------------------------------------
# Git / Versioning / Tagging
# ------------------------------------------------------

vcheck:
	git fetch
	if test "$(LASTTAG)" = "$(VERSION)"; then \
		echo "Warning: tag/version '$(VERSION)' already exists, update HISTORY first. Aborting!"; \
		exit 1; \
	fi

push: vcheck
	echo "tagging git with $(VERSION)"
	git tag $(VERSION)
	git push --tags
	git push

commit: vcheck
	git commit -m "$(LASTCOMMENT)" -a

git: commit push

showv:
	@echo "Git ($(BRANCH)):\\n  Version: $(LASTTAG) (tag)"
	@echo "Local:"
	@echo "  Version: $(VERSION)"
	@echo "  Change:"
	@echo -n "   $(LASTCOMMENT)"

update:
	git pull
	@make clean install
	restart vdr
