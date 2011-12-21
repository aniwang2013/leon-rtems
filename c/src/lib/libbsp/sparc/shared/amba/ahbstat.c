#include <stdint.h>
#include <drvmgr/drvmgr.h>
#include <drvmgr/ambapp_bus.h>

#include <ahbstat.h>

void ahbstat_isr(void *arg);

/* AHB fail interrupt callback to user. This function is declared weak so that
 * the user can define a function pointer variable containing the address
 * responsible for handling errors
 *
 * minor              Index of AHBSTAT hardware
 * regs               Register address of AHBSTAT
 * status             AHBSTAT status register at IRQ
 * failing_address    AHBSTAT Failing address register at IRQ
 *
 * * User return 
 *  0: print error onto terminal with printk and reenable AHBSTAT
 *  1: just re-enable AHBSTAT
 *  2: just print error
 *  3: do nothing, let user do custom handling
 */
int (*ahbstat_error)(
	int minor,
	struct ahbstat_regs *regs,
	uint32_t status,
	uint32_t failing_address
	) __attribute__((weak)) = NULL;

#define AHBSTAT_STS_CE_BIT 9
#define AHBSTAT_STS_NE_BIT 8
#define AHBSTAT_STS_HW_BIT 7
#define AHBSTAT_STS_HM_BIT 3
#define AHBSTAT_STS_HS_BIT 0

#define AHBSTAT_STS_CE (1 << AHBSTAT_STS_CE_BIT)
#define AHBSTAT_STS_NE (1 << AHBSTAT_STS_NE_BIT)
#define AHBSTAT_STS_HW (1 << AHBSTAT_STS_HW_BIT)
#define AHBSTAT_STS_HM (0xf << AHBSTAT_STS_HM_BIT)
#define AHBSTAT_STS_HS (0x7 << AHBSTAT_STS_HS_BIT)

struct ahbstat_priv {
	struct drvmgr_dev *dev;
	struct ahbstat_regs *regs;
	int minor;
	uint32_t last_status;
	uint32_t last_address;
};

int ahbstat_init2(struct drvmgr_dev *dev);

struct drvmgr_drv_ops ahbstat_ops =
{
	.init = {NULL, ahbstat_init2, NULL, NULL},
	.remove = NULL,
	.info = NULL
};

struct amba_dev_id ahbstat_ids[] =
{
	{VENDOR_GAISLER, GAISLER_AHBSTAT},
	{0, 0}		/* Mark end of table */
};

struct amba_drv_info ahbstat_drv_info =
{
	{
		DRVMGR_OBJ_DRV,			/* Driver */
		NULL,				/* Next driver */
		NULL,				/* Device list */
		DRIVER_AMBAPP_GAISLER_AHBSTAT_ID,/* Driver ID */
		"AHBSTAT_DRV",			/* Driver Name */
		DRVMGR_BUS_TYPE_AMBAPP,		/* Bus Type */
		&ahbstat_ops,
		NULL,				/* Funcs */
		0,				/* No devices yet */
		sizeof(struct ahbstat_priv),
	},
	&ahbstat_ids[0]
};

void ahbstat_register_drv (void)
{
	drvmgr_drv_register(&ahbstat_drv_info.general);
}

int ahbstat_init2(struct drvmgr_dev *dev)
{
	struct ahbstat_priv *priv;
	struct amba_dev_info *ambadev;

	priv = dev->priv;
	if (!priv)
		return DRVMGR_NOMEM;
	priv->dev = dev;

	/* Get device information from AMBA PnP information */
	ambadev = (struct amba_dev_info *)dev->businfo;
	if (ambadev == NULL)
		return DRVMGR_FAIL;
	priv->regs = (struct ahbstat_regs *)ambadev->info.apb_slv->start;
	priv->minor = dev->minor_drv;

	/* Initialize hardware */
	priv->regs->status = 0;

	/* Install IRQ handler */
	drvmgr_interrupt_register(dev, 0, "ahbstat", ahbstat_isr, priv);

	return DRVMGR_OK;
}

void ahbstat_isr(void *arg)
{
	struct ahbstat_priv *priv = arg;
	uint32_t fadr, status;
	int rc;

	/* Get hardware status */
	status = priv->regs->status;
	if ((status & AHBSTAT_STS_NE) == 0)
		return;

	/* IRQ generated by AHBSTAT core... handle it here */

	/* Get Failing address */
	fadr = priv->regs->failing;

	priv->last_status = status;
	priv->last_address = fadr;

	/* Let user handle error, default to print the error and reenable HW
	 *
	 * User return 
	 *  0: print error and reenable AHBSTAT
	 *  1: just reenable AHBSTAT
	 *  2: just print error reenable
	 *  3: do nothing
	 */
	rc = 0;
	if (ahbstat_error != NULL)
		rc = ahbstat_error(priv->minor, priv->regs, status, fadr);

	if ((rc & 0x1) == 0) {
		printk("\n### AHBSTAT: %s %s error of size %lu by master %d"
			" at 0x%08lx\n",
			status & AHBSTAT_STS_CE ? "single" : "non-correctable",
			status & AHBSTAT_STS_HW ? "write" : "read",
			(status & AHBSTAT_STS_HS) >> AHBSTAT_STS_HS_BIT,
			(status & AHBSTAT_STS_HM) >> AHBSTAT_STS_HM_BIT,
			fadr);
	}

	if ((rc & 0x2) == 0) {
		/* Trigger new interrupts */
		priv->regs->status = 0;
	}
}

/* Get Last received AHB Error
 *
 * Return
 *   0: No error received
 *   1: Error Received, last status and address stored into argument pointers
 *  -1: No such AHBSTAT device
 */
int ahbstat_last_error(int minor, uint32_t *status, uint32_t *address)
{
	struct drvmgr_dev *dev;
	struct ahbstat_priv *priv;

	if (drvmgr_get_dev(&ahbstat_drv_info.general, minor, &dev)) {
		return -1;
	}
	priv = (struct ahbstat_priv *)dev->priv;

	*status = priv->last_status;
	*address = priv->last_address;

	return (priv->last_status & AHBSTAT_STS_NE) >> AHBSTAT_STS_NE_BIT;
}

/* Get AHBSTAT registers address from minor. NULL returned if no such device */
struct ahbstat_regs *ahbstat_get_regs(int minor)
{
	struct drvmgr_dev *dev;
	struct ahbstat_priv *priv;

	if (drvmgr_get_dev(&ahbstat_drv_info.general, minor, &dev)) {
		return NULL;
	}
	priv = (struct ahbstat_priv *)dev->priv;

	return priv->regs;
}
