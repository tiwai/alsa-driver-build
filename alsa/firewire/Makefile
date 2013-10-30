ifndef SND_TOPDIR
SND_TOPDIR=..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := cmp.c dice.c iso-resources.c isight.c scs1x.c speakers.c

include $(SND_TOPDIR)/alsa-kernel/firewire/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/firewire

include $(SND_TOPDIR)/Rules.make

cmp.c: cmp.patch $(SND_TOPDIR)/alsa-kernel/firewire/cmp.c
dice.c: dice.patch $(SND_TOPDIR)/alsa-kernel/firewire/dice.c
iso-resources.c: iso-resources.patch $(SND_TOPDIR)/alsa-kernel/firewire/iso-resources.c
isight.c: isight.patch $(SND_TOPDIR)/alsa-kernel/firewire/isight.c
scs1x.c: scs1x.patch $(SND_TOPDIR)/alsa-kernel/firewire/scs1x.c
speakers.c: speakers.patch $(SND_TOPDIR)/alsa-kernel/firewire/speakers.c
