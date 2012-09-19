#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>

#if defined(CONFIG_MODVERSIONS) && !defined(__GENKSYMS__) && !defined(__DEPEND__)
#define MODVERSIONS
#include <linux/modversions.h>
#include "sndversions.h"
#endif

#include "config.h"
#include "adriver.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#include "../alsa-kernel/core/memory_wrapper.c"
#else
#include "pci_compat_22.c"
#endif

/* vmalloc_to_page wrapper */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 19)
struct page *snd_compat_vmalloc_to_page(void *pageptr)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long lpage;
	struct page *page;

	lpage = VMALLOC_VMADDR(pageptr);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	spin_lock(&init_mm.page_table_lock);
#endif
	pgd = pgd_offset(&init_mm, lpage);
	pmd = pmd_offset(pgd, lpage);
	pte = pte_offset(pmd, lpage);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	page = virt_to_page(pte_page(*pte));
#else
	page = pte_page(*pte);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	spin_unlock(&init_mm.page_table_lock);
#endif

	return page;
}    
#endif
