#
# Makefile for ALSA driver
# Copyright (c) 1994-98 by Jaroslav Kysela <perex@suse.cz>
#

TOPDIR   = .
ALSAKERNELDIR = ../alsa-kernel

ifeq (Makefile.conf,$(wildcard Makefile.conf))
include Makefile.conf
else
.PHONY: dummy1
dummy1:
	$(MAKE) all-deps
	@echo
	@echo "Please, run the configure script as first..."
	@echo
endif

ifeq (,$(wildcard acinclude.m4))
.PHONY: dummy2
dummy2:
	$(MAKE) all-deps
	@echo
	@echo "Please, run the configure script as first..."
	@echo
endif

SUBDIRS  =
CSUBDIRS =

ifeq (n,$(CONFIG_ISAPNP_KERNEL))
ifeq (y,$(CONFIG_ISAPNP))
ifeq (y,$(CONFIG_SND_ISA))
SUBDIRS  += support
endif
endif
endif

SUBDIRS  += acore i2c drivers isa synth
ifeq (y,$(CONFIG_SND_PCI))
SUBDIRS  += pci
endif
ifeq (y,$(CONFIG_SND_PPC))
SUBDIRS  += ppc
endif
ifeq (y,$(CONFIG_SND_SGI))
SUBDIRS  += hal2
endif
CSUBDIRS += include test utils

.PHONY: all
all: compile

alsa-kernel:
	ln -sf $(ALSAKERNELDIR) alsa-kernel

$(CONFIG_SND_KERNELDIR)/include/linux/pm.h:
	touch $@

include/sound: alsa-kernel $(CONFIG_SND_KERNELDIR)/include/linux/pm.h
	if [ ! -d include/sound ] && [ ! -L include/sound ]; then \
	  ln -sf ../alsa-kernel/include include/sound ; \
	fi

include/sound/version.h: include/sound include/version.h
	cp -auv include/version.h include/sound/version.h

utils/mod-deps: alsa-kernel/scripts/mod-deps.c alsa-kernel/scripts/mod-deps.h
	gcc -Ialsa-kernel/scripts alsa-kernel/scripts/mod-deps.c -o utils/mod-deps

toplevel.config.in: alsa-kernel utils/mod-deps alsa-kernel/scripts/Modules.dep utils/Modules.dep
	cat alsa-kernel/scripts/Modules.dep utils/Modules.dep | utils/mod-deps --makeconf > toplevel.config.in

acinclude.m4: alsa-kernel utils/mod-deps alsa-kernel/scripts/Modules.dep utils/Modules.dep
	cat alsa-kernel/scripts/Modules.dep utils/Modules.dep | utils/mod-deps --acinclude > acinclude.m4

include/config1.h.in: alsa-kernel utils/mod-deps alsa-kernel/scripts/Modules.dep utils/Modules.dep
	cat alsa-kernel/scripts/Modules.dep utils/Modules.dep | utils/mod-deps --include > include/config1.h.in

all-deps: toplevel.config.in acinclude.m4 include/config1.h.in

include/sndversions.h:
	make dep

include/isapnp.h:
	ln -sf ../support/isapnp.h include/isapnp.h

/usr/include/byteswap.h:
	if [ ! -r /usr/include/byteswap.h ]; then \
	  cp utils/patches/byteswap.h /usr/include ; \
	fi

.PHONY: compile
compile: /usr/include/byteswap.h include/sound/version.h include/sndversions.h include/isapnp.h
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d; then exit 1; fi; done
	@echo
	@echo "ALSA modules were successfully compiled."
	@echo

.PHONY: dep
dep: /usr/include/byteswap.h include/sound/version.h include/isapnp.h
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d fastdep; then exit 1; fi; done

.PHONY: map
map:
	awk "{ if ( length( $$1 ) != 0 ) print $$1 }" snd.map | sort -o snd.map1
	mv -f snd.map1 snd.map

.PHONY: install
install: install-modules install-headers install-scripts
	cat WARNING

.PHONY: install-headers
install-headers:
	if [ -L $(DESTDIR)$(prefix)/include/sound ]; then \
		rm -f $(DESTDIR)$(prefix)/include/sound; \
		ln -sf $(SRCDIR)/include/sound $(DESTDIR)$(prefix)/include/sound; \
	else \
		rm -rf $(DESTDIR)$(prefix)/include/sound; \
		install -d -m 755 -g root -o root $(DESTDIR)$(prefix)/include/sound; \
		for f in include/sound/*.h; do \
			install -m 644 -g root -o root $$f $(DESTDIR)$(prefix)/include/sound; \
		done \
	fi

.PHONY: install-modules
install-modules:
	rm -f $(DESTDIR)$(moddir)/snd*.o $(DESTDIR)$(moddir)/persist.o $(DESTDIR)$(moddir)/isapnp.o
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d modules_install; then exit 1; fi; done
ifeq ($(DESTDIR),)
	/sbin/depmod -a
else
	/sbin/depmod -a -b $(DESTDIR)/
endif

.PHONY: install-scripts
install-scripts:
	if [ -d /sbin/init.d ]; then \
	  install -m 755 -g root -o root utils/alsasound $(DESTDIR)/sbin/init.d/alsasound; \
	elif [ -d /etc/rc.d/init.d ]; then \
	  install -m 755 -g root -o root utils/alsasound $(DESTDIR)/etc/rc.d/init.d/alsasound; \
	elif [ -d /etc/init.d ]; then \
	  install -m 755 -g root -o root utils/alsasound $(DESTDIR)/etc/init.d/alsasound; \
	fi

.PHONY: clean
clean:
	rm -f `find . -name ".depend"`
	@for d in $(SUBDIRS); do if ! $(MAKE) -C $$d clean; then exit 1; fi; done
	@for d in $(CSUBDIRS); do if ! $(MAKE) -C $$d clean; then exit 1; fi; done
	rm -f .depend *.o snd.map* *~
	rm -f `find . -name "out.txt"`
	rm -f `find . -name "*.orig"`
	rm -f $(DEXPORT)/*.ver
	rm -f modules/*.o
	rm -f doc/*~

.PHONY: mrproper
mrproper: clean
	rm -f config.cache config.log config.status Makefile.conf
	rm -f utils/alsa-driver.spec

.PHONY: cvsclean
cvsclean: mrproper
	rm -f configure snddevices aclocal.m4 acinclude.m4 include/config.h include/config1.h \
	include/isapnp.h toplevel.conf toplevel.conf.in alsa-kernel include/sound

.PHONY: pack
pack: mrproper
	chmod 755 utils/alsasound
	mv ../alsa-driver ../alsa-driver-$(CONFIG_SND_VERSION)
	tar --exclude=CVS --owner=root --group=root -cvI -C .. -f ../alsa-driver-$(CONFIG_SND_VERSION).tar.bz2 alsa-driver-$(CONFIG_SND_VERSION)
	mv ../alsa-driver-$(CONFIG_SND_VERSION) ../alsa-driver

.PHONY: uninstall
uninstall:
	rm -rf $(DESTDIR)$(prefix)/include/sound
	rm -f $(DESTDIR)$(moddir)/snd*.o $(DESTDIR)$(moddir)/persist.o $(DESTDIR)$(moddir)/isapnp.o
	rm -f $(DESTDIR)/sbin/init.d/alsasound
	rm -f $(DESTDIR)/etc/rc.d/init.d/alsasound
	rm -f $(DESTDIR)/etc/init.d/alsasound
