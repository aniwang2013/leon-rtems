/*
 *  LEON3 Opencores Ethernet MAC Configuration Information
 *
 *  COPYRIGHT (c) 2004.
 *  Gaisler Research
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id$
 */


#include <bsp.h>
#include <libchip/open_eth.h>
#if (OPEN_ETH_DEBUG & OPEN_ETH_DEBUG_PRINT_REGISTERS)
#include <stdio.h>
#endif

/*
 * Default sizes of transmit and receive descriptor areas
 */
#define RDA_COUNT     16
#define TDA_COUNT     16

open_eth_configuration_t leon_open_eth_configuration;

int rtems_leon_open_eth_driver_attach(
  struct rtems_bsdnet_ifconfig *config,
  int attach
)
{
  int device_found = 0;
  int i;
  unsigned int base_addr = 0; /* avoid warnings */
  unsigned int eth_irq = 0;   /* avoid warnings */
  struct ambapp_ahb_info dev;


  /* Scan for MAC AHB slave interface */
  i = ambapp_find_ahbslv(&ambapp_plb, VENDOR_OPENCORES, OPENCORES_ETHMAC, &dev);
  if ( i != 1 ) {
    i = ambapp_find_ahbslv(&ambapp_plb, VENDOR_GAISLER, GAISLER_ETHAHB, &dev);
    if ( i != 1 ) {
      /* No Core found */
      return 0;
    }
  }
  eth_irq = dev.irq + 0x10; /* Calculate vector number from IRQ number */
  base_addr = dev.start[0];

  {
    /* clear control register and reset NIC */
    *(volatile int *) base_addr = 0;
    *(volatile int *) base_addr = 0x800;
    *(volatile int *) base_addr = 0;
    leon_open_eth_configuration.base_address = base_addr;
    leon_open_eth_configuration.vector = eth_irq;
    leon_open_eth_configuration.txd_count = TDA_COUNT;
    leon_open_eth_configuration.rxd_count = RDA_COUNT;
    /* enable 100 MHz operation only if cpu frequency >= 50 MHz */
    if (LEON3_Timer_Regs->scaler_reload >= 49) 
      leon_open_eth_configuration.en100MHz = 1;
    if (rtems_open_eth_driver_attach( config, &leon_open_eth_configuration )) {
      LEON_Clear_interrupt(leon_open_eth_configuration.vector);
      LEON_Unmask_interrupt(leon_open_eth_configuration.vector);
    }
  }
  return 0;
}
