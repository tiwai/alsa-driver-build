#include "adriver.h"

/* workaround for missing prototype */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
struct fw_card;
void fw_schedule_bus_reset(struct fw_card *card, bool delayed,
			   bool short_reset);
#endif

#include "../../alsa-kernel/firewire/bebob/bebob.c"
