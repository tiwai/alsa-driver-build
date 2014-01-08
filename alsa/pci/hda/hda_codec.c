#include "adriver.h"
#include <linux/async.h>
#ifndef ASYNC_DOMAIN_EXCLUSIVE
#define ASYNC_DOMAIN_EXCLUSIVE(name)	LIST_HEAD(name)
#endif
#include "../../alsa-kernel/pci/hda/hda_codec.c"
