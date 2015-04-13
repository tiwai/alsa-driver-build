#include "adriver.h"
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
#include <linux/device.h>
static int pm_runtime_force_suspend(struct device *dev);
static int pm_runtime_force_resume(struct device *dev);
#endif
#include "../../alsa-kernel/pci/hda/hda_codec.c"

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
static int pm_runtime_force_suspend(struct device *dev)
{
	int ret = 0;

	pm_runtime_disable(dev);
	if (pm_runtime_status_suspended(dev))
		return 0;

	if (!device->driver->pm || !device->driver->pm->runtime_suspend) {
		ret = -ENOSYS;
		goto err;
	}

	ret = device->driver->pm->runtime_suspend(dev);
	if (ret)
		goto err;

	pm_runtime_set_suspended(dev);
	return 0;
err:
	pm_runtime_enable(dev);
	return ret;
}

static int pm_runtime_force_resume(struct device *dev)
{
	int ret = 0;

	if (!device->driver->pm || !device->driver->pm->runtime_resume) {
		ret = -ENOSYS;
		goto out;
	}

	ret = device->driver->pm->runtime_resume(dev);
	if (ret)
		goto out;

	pm_runtime_set_active(dev);
	pm_runtime_mark_last_busy(dev);
out:
	pm_runtime_enable(dev);
	return ret;
}
#endif /* <= 3.15 */
