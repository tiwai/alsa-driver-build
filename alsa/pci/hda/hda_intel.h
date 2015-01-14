#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 20, 0)
#include "../../alsa-kernel/pci/hda/hda_intel.h"
#else
#include "hda_intel_compat.h"
#endif
