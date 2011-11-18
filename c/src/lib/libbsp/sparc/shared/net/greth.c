/*
 * Gaisler Research ethernet MAC driver
 * adapted from Opencores driver by Marko Isomaki
 *
 *  The license and distribution terms for this file may be
 *  found in found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *
 *  2008-12-10, Converted to driver manager and added support for
 *              multiple GRETH cores. <daniel@gaisler.com>
 *  2007-09-07, Ported GBIT support from 4.6.5
 */
#include <rtems.h>

#define GRETH_SUPPORTED
#define _KERNEL
#define CPU_U32_FIX
#include <bsp.h>

#include <inttypes.h>
#include <errno.h>
#include <rtems/bspIo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <rtems/error.h>
#include <rtems/rtems_bsdnet.h>

#include <greth.h>
#include <drvmgr/drvmgr.h>
#include <drvmgr/ambapp_bus.h>
#include <ambapp.h>

#include <sys/param.h>
#include <sys/mbuf.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif

#if defined(__m68k__)
extern m68k_isr_entry set_vector( rtems_isr_entry, rtems_vector_number, int );
#else
extern rtems_isr_entry set_vector( rtems_isr_entry, rtems_vector_number, int );
#endif


/* #define GRETH_DEBUG */

#ifdef GRETH_DEBUG
#define DBG(args...) printk(args)
#else
#define DBG(args...)
#endif

#ifdef CPU_U32_FIX
extern void ipalign(struct mbuf *m);
#endif

static inline unsigned int sparc_load_no_cache(unsigned int addr)
{
	unsigned int tmp;
	asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(addr) );
	return tmp;
}
#define NO_CACHE_LOAD(addr) sparc_load_no_cache((unsigned int)addr)

/*
 * Number of OCs supported by this driver
 */
#define NOCDRIVER	1

/*
 * Receive buffer size -- Allow for a full ethernet packet including CRC
 */
#define RBUF_SIZE 1518

#define	ET_MINLEN 64		/* minimum message length */

/*
 * RTEMS event used by interrupt handler to signal driver tasks.
 * This must not be any of the events used by the network task synchronization.
 */
#define INTERRUPT_EVENT	RTEMS_EVENT_1

/*
 * RTEMS event used to start transmit daemon.
 * This must not be the same as INTERRUPT_EVENT.
 */
#define START_TRANSMIT_EVENT	RTEMS_EVENT_2

 /* event to send when tx buffers become available */
#define GRETH_TX_WAIT_EVENT  RTEMS_EVENT_3

#if (MCLBYTES < RBUF_SIZE)
# error "Driver must have MCLBYTES > RBUF_SIZE"
#endif

/* 4s Autonegotiation Timeout */
#ifndef GRETH_AUTONEGO_TIMEOUT_MS
#define GRETH_AUTONEGO_TIMEOUT_MS 4000
#endif

/* For optimizing the autonegotiation time */
#define GRETH_AUTONEGO_PRINT_TIME

/* Ethernet buffer descriptor */

typedef struct _greth_rxtxdesc {
   volatile uint32_t ctrl; /* Length and status */
   uint32_t *addr;         /* Buffer pointer */
} greth_rxtxdesc;


/*
 * Per-device data
 */
struct greth_softc
{

   struct arpcom arpcom;
   struct rtems_drvmgr_dev_info *dev;		/* Driver manager device */
   char devName[32];

   greth_regs *regs;
   int minor;
   int phyaddr;  /* PHY Address configured by user (or -1 to autodetect) */

   int acceptBroadcast;
   rtems_id daemonTid;
   
   unsigned int tx_ptr;
   unsigned int tx_dptr;
   unsigned int tx_cnt;
   unsigned int rx_ptr;
   unsigned int txbufs;
   unsigned int rxbufs;
   greth_rxtxdesc *txdesc;
   greth_rxtxdesc *rxdesc;
   unsigned int txdesc_remote;
   unsigned int rxdesc_remote;
   struct mbuf **rxmbuf;
   struct mbuf **txmbuf;
   rtems_vector_number vector;
   
   /* TX descriptor interrupt generation */
   int tx_int_gen;
   int tx_int_gen_cur;
   struct mbuf *next_tx_mbuf;
   int max_fragsize;
   
   /*Status*/
   struct phy_device_info phydev;
   int fd;
   int sp;
   int gb;
   int gbit_mac;
   int auto_neg;
   unsigned int auto_neg_time;
   
   /*
    * Statistics
    */
   unsigned long rxInterrupts;
   
   unsigned long rxPackets;
   unsigned long rxLengthError;
   unsigned long rxNonOctet;
   unsigned long rxBadCRC;
   unsigned long rxOverrun;
   
   unsigned long txInterrupts;
   
   unsigned long txDeferred;
   unsigned long txHeartbeat;
   unsigned long txLateCollision;
   unsigned long txRetryLimit;
   unsigned long txUnderrun;

};

int greth_process_tx_gbit(struct greth_softc *sc);
int greth_process_tx(struct greth_softc *sc);

static char *almalloc(int sz)
{
        char *tmp;
        tmp = calloc(1,2*sz);
        tmp = (char *) (((int)tmp+sz) & ~(sz -1));
        return(tmp);
}

/* GRETH interrupt handler */

void greth_interrupt (int irq, void *arg)
{
        uint32_t status;
        uint32_t ctrl;
        rtems_event_set events = 0;
        struct greth_softc *greth = arg;
        
        /* read and clear interrupt cause */
        status = greth->regs->status;
        greth->regs->status = status;
        ctrl = greth->regs->ctrl;
        
        /* Frame received? */
        if ((ctrl & GRETH_CTRL_RXIRQ) && (status & (GRETH_STATUS_RXERR | GRETH_STATUS_RXIRQ)))
        {
                greth->rxInterrupts++;
                /* Stop RX-Error and RX-Packet interrupts */
                ctrl &= ~GRETH_CTRL_RXIRQ;
                events |= INTERRUPT_EVENT;
        }
        
        if ( (ctrl & GRETH_CTRL_TXIRQ) && (status & (GRETH_STATUS_TXERR | GRETH_STATUS_TXIRQ)) )
        {
                greth->txInterrupts++;
                ctrl &= ~GRETH_CTRL_TXIRQ;
                events |= GRETH_TX_WAIT_EVENT;
        }
        
        /* Clear interrupt sources */
        greth->regs->ctrl = ctrl;
        
        /* Send the event(s) */
        if ( events )
            rtems_event_send (greth->daemonTid, events);
}

static uint32_t read_mii(struct greth_softc *sc, uint32_t phy_addr, uint32_t reg_addr)
{
    while (sc->regs->mdio_ctrl & GRETH_MDIO_BUSY) {}
    sc->regs->mdio_ctrl = (phy_addr << 11) | (reg_addr << 6) | GRETH_MDIO_READ;
    while (sc->regs->mdio_ctrl & GRETH_MDIO_BUSY) {}
    if (!(sc->regs->mdio_ctrl & GRETH_MDIO_LINKFAIL))
        return((sc->regs->mdio_ctrl >> 16) & 0xFFFF);
    else {
	printf("greth: failed to read mii\n");
	return (0);
    }
}

static void write_mii(struct greth_softc *sc, uint32_t phy_addr, uint32_t reg_addr, uint32_t data)
{
    while (sc->regs->mdio_ctrl & GRETH_MDIO_BUSY) {}
    sc->regs->mdio_ctrl =
     ((data & 0xFFFF) << 16) | (phy_addr << 11) | (reg_addr << 6) | GRETH_MDIO_WRITE;
    while (sc->regs->mdio_ctrl & GRETH_MDIO_BUSY) {}
}

static void print_init_info(struct greth_softc *sc) 
{
    printf("greth: driver attached\n");
    if ( sc->auto_neg == -1 ){
        printf("Auto negotiation timed out. Selecting default config\n");
    }
    printf("**** PHY ****\n");
    printf("Vendor: %x   Device: %x   Revision: %d\n",sc->phydev.vendor, sc->phydev.device, sc->phydev.rev);
    printf("Current Operating Mode: ");
    if (sc->gb) {
        printf("1000 Mbit ");
    } else if (sc->sp) {
        printf("100 Mbit ");
    } else {
        printf("10 Mbit ");
    }
    if (sc->fd) {
        printf("Full Duplex\n");
    } else {
        printf("Half Duplex\n");
    }
#ifdef GRETH_AUTONEGO_PRINT_TIME
    if ( sc->auto_neg ){
        printf("Autonegotiation Time: %dms\n",sc->auto_neg_time);
    }
#endif
}


/*
 * Initialize the ethernet hardware
 */
static void
greth_initialize_hardware (struct greth_softc *sc)
{
    struct mbuf *m;
    int i;
    int phyaddr;
    int phyctrl;
    int phystatus;
    int tmp1;
    int tmp2;
    unsigned int msecs;
    struct timeval tstart, tnow;

    greth_regs *regs;

    regs = sc->regs;
    
    /* Reset the controller.  */
    sc->rxInterrupts = 0;
    sc->rxPackets = 0;

    regs->ctrl = 0;
    regs->ctrl = GRETH_CTRL_RST;	/* Reset ON */
    regs->ctrl = 0;			/* Reset OFF */
    
    /* Check if mac is gbit capable*/
    sc->gbit_mac = (regs->ctrl >> 27) & 1;
    
    /* Get the phy address which assumed to have been set
       correctly with the reset value in hardware*/
    if ( sc->phyaddr == -1 ) {
        phyaddr = (regs->mdio_ctrl >> 11) & 0x1F;
    } else {
        phyaddr = sc->phyaddr;
    }

    /* get phy control register default values */
    while ((phyctrl = read_mii(sc, phyaddr, 0)) & 0x8000) {}
    
    /* reset PHY and wait for completion */
    write_mii(sc, phyaddr, 0, 0x8000 | phyctrl);

    while ((read_mii(sc, phyaddr, 0)) & 0x8000) {}
    
    /* Check if PHY is autoneg capable and then determine operating mode, 
       otherwise force it to 10 Mbit halfduplex */
    sc->gb = 0;
    sc->fd = 0;
    sc->sp = 0;
    sc->auto_neg = 0;
    sc->auto_neg_time = 0;
    if ((phyctrl >> 12) & 1) {
            /*wait for auto negotiation to complete*/
            msecs = 0;
            sc->auto_neg = 1;
            if ( rtems_clock_get_tod_timeval(&tstart) == RTEMS_NOT_DEFINED){
                /* Not inited, set to epoch */
                rtems_time_of_day time;
                time.year   = 1988;
                time.month  = 1;
                time.day    = 1;
                time.hour   = 0;
                time.minute = 0;
                time.second = 0;
                time.ticks  = 0;
                rtems_clock_set(&time);

                tstart.tv_sec = 0;
                tstart.tv_usec = 0;
                rtems_clock_get_tod_timeval(&tstart);
            }
            while (!(((phystatus = read_mii(sc, phyaddr, 1)) >> 5) & 1)) {
                    if ( rtems_clock_get_tod_timeval(&tnow) != RTEMS_SUCCESSFUL )
                      printk("rtems_clock_get_tod_timeval failed\n\r");
                    msecs = (tnow.tv_sec-tstart.tv_sec)*1000+(tnow.tv_usec-tstart.tv_usec)/1000;
                    if ( msecs > GRETH_AUTONEGO_TIMEOUT_MS ){
                            sc->auto_neg_time = msecs;
                            sc->auto_neg = -1; /* Failed */
                            tmp1 = read_mii(sc, phyaddr, 0);
                            sc->gb = ((phyctrl >> 6) & 1) && !((phyctrl >> 13) & 1);
                            sc->sp = !((phyctrl >> 6) & 1) && ((phyctrl >> 13) & 1);
                            sc->fd = (phyctrl >> 8) & 1;
                            goto auto_neg_done;
                    }
            }
            sc->auto_neg_time = msecs;
            sc->phydev.adv = read_mii(sc, phyaddr, 4);
            sc->phydev.part = read_mii(sc, phyaddr, 5);
            if ((phystatus >> 8) & 1) {
                    sc->phydev.extadv = read_mii(sc, phyaddr, 9);
                    sc->phydev.extpart = read_mii(sc, phyaddr, 10);
                       if ( (sc->phydev.extadv & GRETH_MII_EXTADV_1000FD) &&
                            (sc->phydev.extpart & GRETH_MII_EXTPRT_1000FD)) {
                               sc->gb = 1;
                               sc->fd = 1;
                       }
                       if ( (sc->phydev.extadv & GRETH_MII_EXTADV_1000HD) &&
                            (sc->phydev.extpart & GRETH_MII_EXTPRT_1000HD)) {
                               sc->gb = 1;
                               sc->fd = 0;
                       }
            }
            if ((sc->gb == 0) || ((sc->gb == 1) && (sc->gbit_mac == 0))) {
                    if ( (sc->phydev.adv & GRETH_MII_100TXFD) &&
                         (sc->phydev.part & GRETH_MII_100TXFD)) {
                            sc->sp = 1;
                            sc->fd = 1;
                    }
                    if ( (sc->phydev.adv & GRETH_MII_100TXHD) &&
                         (sc->phydev.part & GRETH_MII_100TXHD)) {
                            sc->sp = 1;
                            sc->fd = 0;
                    }
                    if ( (sc->phydev.adv & GRETH_MII_10FD) &&
                         (sc->phydev.part & GRETH_MII_10FD)) {
                            sc->fd = 1;
                    }
            }
    }
auto_neg_done:
    sc->phydev.vendor = 0;
    sc->phydev.device = 0;
    sc->phydev.rev = 0;
    phystatus = read_mii(sc, phyaddr, 1);
    
    /*Read out PHY info if extended registers are available */
    if (phystatus & 1) {  
            tmp1 = read_mii(sc, phyaddr, 2);
            tmp2 = read_mii(sc, phyaddr, 3);

            sc->phydev.vendor = (tmp1 << 6) | ((tmp2 >> 10) & 0x3F);
            sc->phydev.rev = tmp2 & 0xF;
            sc->phydev.device = (tmp2 >> 4) & 0x3F;
    }

    /* Force to 10 mbit half duplex if the 10/100 MAC is used with a 1000 PHY*/
    /*check if marvell 88EE1111 PHY. Needs special reset handling */
    if ((phystatus & 1) && (sc->phydev.vendor == 0x005043) && (sc->phydev.device == 0x0C)) {
            if (((sc->gb) && !(sc->gbit_mac)) || !((phyctrl >> 12) & 1)) {
                    write_mii(sc, phyaddr, 0, sc->sp << 13);
                    write_mii(sc, phyaddr, 0, 0x8000);
                    sc->gb = 0;
                    sc->sp = 0;
                    sc->fd = 0;
            }
    } else {
            if (((sc->gb) && !(sc->gbit_mac))  || !((phyctrl >> 12) & 1)) {
                    write_mii(sc, phyaddr, 0, sc->sp << 13);
                    sc->gb = 0;
                    sc->sp = 0;
                    sc->fd = 0;
            }
    }
    while ((read_mii(sc, phyaddr, 0)) & 0x8000) {}

    regs->ctrl = 0;
    regs->ctrl = GRETH_CTRL_RST;	/* Reset ON */
    regs->ctrl = 0;

    /* Initialize rx/tx descriptor pointers */
    sc->txdesc = (greth_rxtxdesc *) almalloc(1024);
    sc->rxdesc = (greth_rxtxdesc *) almalloc(1024);
    sc->tx_ptr = 0;
    sc->tx_dptr = 0;
    sc->tx_cnt = 0;
    sc->rx_ptr = 0;

    /* Translate the base address into an address that the BRM core can understand */
    rtems_drvmgr_mmap_translate(sc->dev, 0, (void *)sc->txdesc, (void **)&sc->txdesc_remote);
    rtems_drvmgr_mmap_translate(sc->dev, 0, (void *)sc->rxdesc, (void **)&sc->rxdesc_remote);
    regs->txdesc = (int) sc->txdesc_remote;
    regs->rxdesc = (int) sc->rxdesc_remote;

    sc->rxmbuf = calloc(sc->rxbufs, sizeof(*sc->rxmbuf));
    sc->txmbuf = calloc(sc->txbufs, sizeof(*sc->txmbuf));

    for (i = 0; i < sc->txbufs; i++)
      {
              sc->txdesc[i].ctrl = 0;
              if (!(sc->gbit_mac)) {
                      rtems_drvmgr_mmap_translate(sc->dev, 0, (void *)malloc(GRETH_MAXBUF_LEN), (void **)&sc->txdesc[i].addr);
              }
#ifdef GRETH_DEBUG
              /* printf("TXBUF: %08x\n", (int) sc->txdesc[i].addr); */
#endif
      }
    for (i = 0; i < sc->rxbufs; i++)
      {
         MGETHDR (m, M_WAIT, MT_DATA);
          MCLGET (m, M_WAIT);
          if (sc->gbit_mac)
                  m->m_data += 2;
	  m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
          sc->rxmbuf[i] = m;
          rtems_drvmgr_mmap_translate(sc->dev, 0, (void *)mtod(m, uint32_t *), (void **)&sc->rxdesc[i].addr);
          sc->rxdesc[i].ctrl = GRETH_RXD_ENABLE | GRETH_RXD_IRQ;
#ifdef GRETH_DEBUG
/* 	  printf("RXBUF: %08x\n", (int) sc->rxdesc[i].addr); */
#endif
      }
    sc->rxdesc[sc->rxbufs - 1].ctrl |= GRETH_RXD_WRAP;

    /* set ethernet address.  */
    regs->mac_addr_msb = 
      sc->arpcom.ac_enaddr[0] << 8 | sc->arpcom.ac_enaddr[1];
    regs->mac_addr_lsb = 
      sc->arpcom.ac_enaddr[2] << 24 | sc->arpcom.ac_enaddr[3] << 16 |
      sc->arpcom.ac_enaddr[4] << 8 | sc->arpcom.ac_enaddr[5];

    if ( sc->rxbufs < 10 ) {
        sc->tx_int_gen = sc->tx_int_gen_cur = 1;
    }else{
        sc->tx_int_gen = sc->tx_int_gen_cur = sc->txbufs/2;
    }
    sc->next_tx_mbuf = NULL;
    
    if ( !sc->gbit_mac )
        sc->max_fragsize = 1;

    /* clear all pending interrupts */
    regs->status = 0xffffffff;
    
    /* install interrupt handler */
    rtems_drvmgr_interrupt_enable(sc->dev, 0, greth_interrupt, sc);

    regs->ctrl |= GRETH_CTRL_RXEN | (sc->fd << 4) | GRETH_CTRL_RXIRQ | (sc->sp << 7) | (sc->gb << 8);

    print_init_info(sc);
}

#ifdef CPU_U32_FIX

/*
 * Routine to align the received packet so that the ip header
 * is on a 32-bit boundary. Necessary for cpu's that do not
 * allow unaligned loads and stores and when the 32-bit DMA
 * mode is used.
 *
 * Transfers are done on word basis to avoid possibly slow byte
 * and half-word writes.
 */

void ipalign(struct mbuf *m)
{
  unsigned int *first, *last, data;
  unsigned int tmp = 0;

  if ((((int) m->m_data) & 2) && (m->m_len)) {
    last = (unsigned int *) ((((int) m->m_data) + m->m_len + 8) & ~3);
    first = (unsigned int *) (((int) m->m_data) & ~3);
		/* tmp = *first << 16; */
		asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(first) );
		tmp = tmp << 16;
    first++;
    do {
			/* When snooping is not available the LDA instruction must be used
			 * to avoid the cache to return an illegal value.
			 ** Load with forced cache miss
			 * data = *first; 
			 */
      asm volatile (" lda [%1] 1, %0\n" : "=r"(data) : "r"(first) );
      *first = tmp | (data >> 16);
      tmp = data << 16;
      first++;
    } while (first <= last);

    m->m_data = (caddr_t)(((int) m->m_data) + 2);
  }
}
#endif

void
greth_Daemon (void *arg)
{
    struct ether_header *eh;
    struct greth_softc *dp = (struct greth_softc *) arg;
    struct ifnet *ifp = &dp->arpcom.ac_if;
    struct mbuf *m;
    unsigned int len, len_status, bad;
    rtems_event_set events;
    rtems_interrupt_level level;
    int first;
		int tmp;
		unsigned int addr;
    
    for (;;)
      {
        rtems_bsdnet_event_receive (INTERRUPT_EVENT | GRETH_TX_WAIT_EVENT,
                                    RTEMS_WAIT | RTEMS_EVENT_ANY,
                                    RTEMS_NO_TIMEOUT, &events);
        
        if ( events & GRETH_TX_WAIT_EVENT ){
            /* TX interrupt.
             * We only end up here when all TX descriptors has been used,
             * and 
             */
            if ( dp->gbit_mac )
                greth_process_tx_gbit(dp);
            else
                greth_process_tx(dp);
            
            /* If we didn't get a RX interrupt we don't process it */
            if ( (events & INTERRUPT_EVENT) == 0 )
                continue;
        }
        
        
#ifdef GRETH_ETH_DEBUG
    printf ("r\n");
#endif
    first=1;
    /* Scan for Received packets */
again:
    while (!((len_status =
		    NO_CACHE_LOAD(&dp->rxdesc[dp->rx_ptr].ctrl)) & GRETH_RXD_ENABLE))
	    {
                    bad = 0;
                    if (len_status & GRETH_RXD_TOOLONG)
                    {
                            dp->rxLengthError++;
                            bad = 1;
                    }
                    if (len_status & GRETH_RXD_DRIBBLE)
                    {
                            dp->rxNonOctet++;
                            bad = 1;
                    }
                    if (len_status & GRETH_RXD_CRCERR)
                    {
                            dp->rxBadCRC++;
                            bad = 1;
                    }
                    if (len_status & GRETH_RXD_OVERRUN)
                    {
                            dp->rxOverrun++;
                            bad = 1;
                    }
                    if (len_status & GRETH_RXD_LENERR)
                    {
                            dp->rxLengthError++;
                            bad = 1;
                    }
                    if (!bad)
                    {
                            /* pass on the packet in the receive buffer */
                            len = len_status & 0x7FF;
                            m = dp->rxmbuf[dp->rx_ptr];
#ifdef GRETH_DEBUG
                            int i;
                            printf("RX: 0x%08x, Len: %d : ", (int) m->m_data, len);
                            for (i=0; i<len; i++)
                                    printf("%x%x", (m->m_data[i] >> 4) & 0x0ff, m->m_data[i] & 0x0ff);
                            printf("\n");
#endif
                            m->m_len = m->m_pkthdr.len =
                                    len - sizeof (struct ether_header);

                            eh = mtod (m, struct ether_header *);

                            m->m_data += sizeof (struct ether_header);
#ifdef CPU_U32_FIX
                            if(!dp->gbit_mac) {
                                    /* OVERRIDE CACHED ETHERNET HEADER FOR NON-SNOOPING SYSTEMS */
                                    addr = (unsigned int)eh;
                                    asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(addr) );
                                    addr+=4;
                                    asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(addr) );
                                    addr+=4;
                                    asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(addr) );
                                    addr+=4;
                                    asm volatile (" lda [%1] 1, %0\n" : "=r"(tmp) : "r"(addr) );

                                    ipalign(m);	/* Align packet on 32-bit boundary */
                            }
#endif
/*
                            if(!(dp->gbit_mac) && !CPU_SPARC_HAS_SNOOPING) {
                                    rtems_cache_invalidate_entire_data();
                            }
*/
                            ether_input (ifp, eh, m);
                            MGETHDR (m, M_WAIT, MT_DATA);
                            MCLGET (m, M_WAIT);
                            if (dp->gbit_mac)
                                    m->m_data += 2;
                            dp->rxmbuf[dp->rx_ptr] = m;
                            m->m_pkthdr.rcvif = ifp;
                            rtems_drvmgr_mmap_translate(dp->dev, 0, (void *)mtod (m, uint32_t *), (void **)&dp->rxdesc[dp->rx_ptr].addr);
                            dp->rxPackets++;
                    }
                    if (dp->rx_ptr == dp->rxbufs - 1) {
                            dp->rxdesc[dp->rx_ptr].ctrl = GRETH_RXD_ENABLE | GRETH_RXD_IRQ | GRETH_RXD_WRAP;
                    } else {
                            dp->rxdesc[dp->rx_ptr].ctrl = GRETH_RXD_ENABLE | GRETH_RXD_IRQ;
                    }
                    rtems_interrupt_disable(level);
                    dp->regs->ctrl |= GRETH_CTRL_RXEN;
                    rtems_interrupt_enable(level);
                    dp->rx_ptr = (dp->rx_ptr + 1) % dp->rxbufs;
            }
        
        /* Always scan twice to avoid deadlock */
        if ( first ){
            first=0;
            rtems_interrupt_disable(level);
            dp->regs->ctrl |= GRETH_CTRL_RXIRQ;
            rtems_interrupt_enable(level);
            goto again;
        }

      }
    
}

static int inside = 0;
static int
sendpacket (struct ifnet *ifp, struct mbuf *m)
{
    struct greth_softc *dp = ifp->if_softc;
    unsigned char *temp;
    struct mbuf *n;
    unsigned int len;
    rtems_interrupt_level level;
        
    /*printf("Send packet entered\n");*/
    if (inside) printf ("error: sendpacket re-entered!!\n");
    inside = 1;
    
    /*
     * Is there a free descriptor available?
     */
    if ( NO_CACHE_LOAD(&dp->txdesc[dp->tx_ptr].ctrl) & GRETH_TXD_ENABLE ){
            /* No. */
            inside = 0;
            return 1;
    }
    
    /* Remember head of chain */
    n = m;

    len = 0;
    temp = (unsigned char *) NO_CACHE_LOAD(&dp->txdesc[dp->tx_ptr].addr);
    rtems_drvmgr_mmap_translate(dp->dev, 1, (void *)temp, (void **)&temp);
#ifdef GRETH_DEBUG
    printf("TXD: 0x%08x : BUF: 0x%08x\n", (int) m->m_data, (int) temp);
#endif
    for (;;)
    {
#ifdef GRETH_DEBUG
            int i;
            printf("MBUF: 0x%08x : ", (int) m->m_data);
            for (i=0;i<m->m_len;i++)
                    printf("%x%x", (m->m_data[i] >> 4) & 0x0ff, m->m_data[i] & 0x0ff);
            printf("\n");
#endif
            len += m->m_len;
            if (len <= RBUF_SIZE)
                    memcpy ((void *) temp, (char *) m->m_data, m->m_len);
            temp += m->m_len;
            if ((m = m->m_next) == NULL)
                    break;
    }
    
    m_freem (n);
    
    /* don't send long packets */

    if (len <= GRETH_MAXBUF_LEN) {
            if (dp->tx_ptr < dp->txbufs-1) {
                    dp->txdesc[dp->tx_ptr].ctrl = GRETH_TXD_ENABLE | len;
            } else {
                    dp->txdesc[dp->tx_ptr].ctrl = 
                            GRETH_TXD_WRAP | GRETH_TXD_ENABLE | len;
            }
            dp->tx_ptr = (dp->tx_ptr + 1) % dp->txbufs;
            rtems_interrupt_disable(level);
            dp->regs->ctrl = dp->regs->ctrl | GRETH_CTRL_TXEN;
            rtems_interrupt_enable(level);
            
    }
    inside = 0;
    
    return 0;
}


int
sendpacket_gbit (struct ifnet *ifp, struct mbuf *m)
{
        struct greth_softc *dp = ifp->if_softc;
        unsigned int len;
        
        unsigned int ctrl;
        int frags;
        struct mbuf *mtmp;
        int int_en;
        rtems_interrupt_level level;

        if (inside) printf ("error: sendpacket re-entered!!\n");
        inside = 1;
        
        len = 0;
#ifdef GRETH_DEBUG
        printf("TXD: 0x%08x\n", (int) m->m_data);
#endif
        /* Get number of fragments too see if we have enough
         * resources.
         */
        frags=1;
        mtmp=m;
        while(mtmp->m_next){
            frags++;
            mtmp = mtmp->m_next;
        }

        if ( frags > dp->max_fragsize ) 
            dp->max_fragsize = frags;
        
        if ( frags > dp->txbufs ){
            inside = 0;
            printf("GRETH: MBUF-chain cannot be sent. Increase descriptor count.\n");
            return -1;
        }
        
        if ( frags > (dp->txbufs-dp->tx_cnt) ){
            inside = 0;
            /* Return number of fragments */
            return frags;
        }
        
        
        /* Enable interrupt from descriptor every tx_int_gen
         * descriptor. Typically every 16 descriptor. This
         * is only to reduce the number of interrupts during
         * heavy load.
         */
        dp->tx_int_gen_cur-=frags;
        if ( dp->tx_int_gen_cur <= 0 ){
            dp->tx_int_gen_cur = dp->tx_int_gen;
            int_en = GRETH_TXD_IRQ;
        }else{
            int_en = 0;
        }
        
        /* At this stage we know that enough descriptors are available */
        for (;;)
        {
                
#ifdef GRETH_DEBUG
            int i;
            printf("MBUF: 0x%08x, Len: %d : ", (int) m->m_data, m->m_len);
            for (i=0; i<m->m_len; i++)
                printf("%x%x", (m->m_data[i] >> 4) & 0x0ff, m->m_data[i] & 0x0ff);
            printf("\n");
#endif
            len += m->m_len;
            rtems_drvmgr_mmap_translate(dp->dev, 0, (void *)(uint32_t *)m->m_data, (void **)&dp->txdesc[dp->tx_ptr].addr);

            /* Wrap around? */
            if (dp->tx_ptr < dp->txbufs-1) {
                ctrl = GRETH_TXD_ENABLE | GRETH_TXD_CS;
            }else{
                ctrl = GRETH_TXD_ENABLE | GRETH_TXD_CS | GRETH_TXD_WRAP;
            }

            /* Enable Descriptor */  
            if ((m->m_next) == NULL) {
                dp->txdesc[dp->tx_ptr].ctrl = ctrl | int_en | m->m_len;
                break;
            }else{
                dp->txdesc[dp->tx_ptr].ctrl = GRETH_TXD_MORE | ctrl | int_en | m->m_len;
            }

            /* Next */
            dp->txmbuf[dp->tx_ptr] = m;
            dp->tx_ptr = (dp->tx_ptr + 1) % dp->txbufs;
            dp->tx_cnt++;
            m = m->m_next;
        }
        dp->txmbuf[dp->tx_ptr] = m;
        dp->tx_ptr = (dp->tx_ptr + 1) % dp->txbufs;
        dp->tx_cnt++;
      
        /* Tell Hardware about newly enabled descriptor */
        rtems_interrupt_disable(level);
        dp->regs->ctrl = dp->regs->ctrl | GRETH_CTRL_TXEN;
        rtems_interrupt_enable(level);

        inside = 0;
               
        return 0;
}

int greth_process_tx_gbit(struct greth_softc *sc)
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct mbuf *m;
    rtems_interrupt_level level;
    int first=1;
    
    /*
     * Send packets till queue is empty
     */
    for (;;){
        /* Reap Sent packets */
        while((sc->tx_cnt > 0) && !(NO_CACHE_LOAD(&sc->txdesc[sc->tx_dptr].ctrl) & GRETH_TXD_ENABLE)) {
            m_free(sc->txmbuf[sc->tx_dptr]);
            sc->tx_dptr = (sc->tx_dptr + 1) % sc->txbufs;
            sc->tx_cnt--;
        }
        
        if ( sc->next_tx_mbuf ){
            /* Get packet we tried but faild to transmit last time */
            m = sc->next_tx_mbuf;
            sc->next_tx_mbuf = NULL; /* Mark packet taken */
        }else{
            /*
             * Get the next mbuf chain to transmit from Stack.
             */
            IF_DEQUEUE (&ifp->if_snd, m);
            if (!m){
                /* Hardware has sent all schedule packets, this
                 * makes the stack enter at greth_start next time
                 * a packet is to be sent.
                 */
                ifp->if_flags &= ~IFF_OACTIVE;
                break;
            }
        }

        /* Are there free descriptors available? */
        /* Try to send packet, if it a negative number is returned. */
        if ( (sc->tx_cnt >= sc->txbufs) || sendpacket_gbit(ifp, m) ){
            /* Not enough resources */
             
            /* Since we have taken the mbuf out of the "send chain"
             * we must remember to use that next time we come back.
             * or else we have dropped a packet.
             */
            sc->next_tx_mbuf = m;
            
            /* Not enough resources, enable interrupt for transmissions
             * this way we will be informed when more TX-descriptors are 
             * available.
             */
            if ( first ){
                first = 0;
                rtems_interrupt_disable(level);
                ifp->if_flags |= IFF_OACTIVE;
                sc->regs->ctrl |= GRETH_CTRL_TXIRQ;
                rtems_interrupt_enable(level);
                
                /* We must check again to be sure that we didn't 
                 * miss an interrupt (if a packet was sent just before
                 * enabling interrupts)
                 */
                continue;
            }
            
            return -1;
        }else{
            /* Sent Ok, proceed to process more packets if available */
        }
    }
    return 0;
}

int greth_process_tx(struct greth_softc *sc)
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct mbuf *m;
    rtems_interrupt_level level;
    int first=1;
    
    /*
     * Send packets till queue is empty
     */
    for (;;){
        if ( sc->next_tx_mbuf ){
            /* Get packet we tried but failed to transmit last time */
            m = sc->next_tx_mbuf;
            sc->next_tx_mbuf = NULL; /* Mark packet taken */
        }else{
            /*
             * Get the next mbuf chain to transmit from Stack.
             */
            IF_DEQUEUE (&ifp->if_snd, m);
            if (!m){
                /* Hardware has sent all schedule packets, this
                 * makes the stack enter at greth_start next time
                 * a packet is to be sent.
                 */
                ifp->if_flags &= ~IFF_OACTIVE;
                break;
            }
        }

        /* Try to send packet, failed if it a non-zero number is returned. */
        if ( sendpacket(ifp, m) ){
            /* Not enough resources */
             
            /* Since we have taken the mbuf out of the "send chain"
             * we must remember to use that next time we come back.
             * or else we have dropped a packet.
             */
            sc->next_tx_mbuf = m;
            
            /* Not enough resources, enable interrupt for transmissions
             * this way we will be informed when more TX-descriptors are 
             * available.
             */
            if ( first ){
                first = 0;
                rtems_interrupt_disable(level);
                ifp->if_flags |= IFF_OACTIVE;
                sc->regs->ctrl |= GRETH_CTRL_TXIRQ;
                rtems_interrupt_enable(level);
                
                /* We must check again to be sure that we didn't 
                 * miss an interrupt (if a packet was sent just before
                 * enabling interrupts)
                 */
                continue;
            }
            
            return -1;
        }else{
            /* Sent Ok, proceed to process more packets if available */
        }
    }
    return 0;
}

static void
greth_start (struct ifnet *ifp)
{
    struct greth_softc *sc = ifp->if_softc;
    
    if ( ifp->if_flags & IFF_OACTIVE )
            return;
    
    if ( sc->gbit_mac ){
        /* No use trying to handle this if we are waiting on GRETH
         * to send the previously scheduled packets.
         */
        
        greth_process_tx_gbit(sc);
    }else{
        greth_process_tx(sc);
    }
    
}

/*
 * Initialize and start the device
 */
static void
greth_init (void *arg)
{
    struct greth_softc *sc = arg;
    struct ifnet *ifp = &sc->arpcom.ac_if;

    if (sc->daemonTid == 0)
      {

	  /*
	   * Start driver tasks
	   */
	  sc->daemonTid = rtems_bsdnet_newproc ("DCrxtx", 4096,
						  greth_Daemon, sc);

	  /*
	   * Set up GRETH hardware
	   */
      greth_initialize_hardware (sc);
          
      }

    /*
     * Tell the world that we're running.
     */
    ifp->if_flags |= IFF_RUNNING;

}

/*
 * Stop the device
 */
static void
greth_stop (struct greth_softc *sc)
{
    struct ifnet *ifp = &sc->arpcom.ac_if;

    ifp->if_flags &= ~IFF_RUNNING;

    sc->regs->ctrl = 0;		        /* RX/TX OFF */
    sc->regs->ctrl = GRETH_CTRL_RST;	/* Reset ON */
    sc->regs->ctrl = 0;	         	/* Reset OFF */
    
    sc->next_tx_mbuf = NULL;
}


/*
 * Show interface statistics
 */
static void
greth_stats (struct greth_softc *sc)
{
  printf ("      Rx Interrupts:%-8lu", sc->rxInterrupts);
  printf ("      Rx Packets:%-8lu", sc->rxPackets);
  printf ("          Length:%-8lu", sc->rxLengthError);
  printf ("       Non-octet:%-8lu\n", sc->rxNonOctet);
  printf ("            Bad CRC:%-8lu", sc->rxBadCRC);
  printf ("         Overrun:%-8lu", sc->rxOverrun);
  printf ("      Tx Interrupts:%-8lu", sc->txInterrupts);
  printf ("      Maximal Frags:%-8d", sc->max_fragsize);
  printf ("      GBIT MAC:%-8d", sc->gbit_mac);
}

/*
 * Driver ioctl handler
 */
static int
greth_ioctl (struct ifnet *ifp, ioctl_command_t command, caddr_t data)
{
    struct greth_softc *sc = ifp->if_softc;
    int error = 0;

    switch (command)
      {
      case SIOCGIFADDR:
      case SIOCSIFADDR:
	  ether_ioctl (ifp, command, data);
	  break;

      case SIOCSIFFLAGS:
	  switch (ifp->if_flags & (IFF_UP | IFF_RUNNING))
	    {
	    case IFF_RUNNING:
		greth_stop (sc);
                break;

	    case IFF_UP:
		greth_init (sc);
		break;

	    case IFF_UP | IFF_RUNNING:
		greth_stop (sc);
		greth_init (sc);
		break;
       default:
		break;
	    }
	  break;

      case SIO_RTEMS_SHOW_STATS:
	  greth_stats (sc);
	  break;

	  /*
	   * FIXME: All sorts of multicast commands need to be added here!
	   */
      default:
	  error = EINVAL;
	  break;
      }

    return error;
}

/*
 * Attach an GRETH driver to the system
 */
int
greth_interface_driver_attach (
    struct rtems_bsdnet_ifconfig *config,
    int attach
    )
{
    struct greth_softc *sc;
    struct ifnet *ifp;
    int mtu;
    int unitNumber;
    char *unitName;
    
      /* parse driver name */
    if ((unitNumber = rtems_bsdnet_parse_driver_name (config, &unitName)) < 0)
	return 0;

    sc = config->drv_ctrl;
    ifp = &sc->arpcom.ac_if;
#ifdef GRETH_DEBUG
    printf("GRETH[%d]: %s, sc %p, dev %p on %s\n", unitNumber, config->ip_address, sc, sc->dev, sc->dev->parent->dev->name);
#endif
    if (config->hardware_address)
      {
	  memcpy (sc->arpcom.ac_enaddr, config->hardware_address,
		  ETHER_ADDR_LEN);
      }
    else
      {
	  memset (sc->arpcom.ac_enaddr, 0x08, ETHER_ADDR_LEN);
      }

    if (config->mtu)
	mtu = config->mtu;
    else
	mtu = ETHERMTU;

    sc->acceptBroadcast = !config->ignore_broadcast;

    /*
     * Set up network interface values
     */
    ifp->if_softc = sc;
    ifp->if_unit = unitNumber;
    ifp->if_name = unitName;
    ifp->if_mtu = mtu;
    ifp->if_init = greth_init;
    ifp->if_ioctl = greth_ioctl;
    ifp->if_start = greth_start;
    ifp->if_output = ether_output;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
    if (ifp->if_snd.ifq_maxlen == 0)
	ifp->if_snd.ifq_maxlen = ifqmaxlen;

    /*
     * Attach the interface
     */
    if_attach (ifp);
    ether_ifattach (ifp);

#ifdef GRETH_DEBUG
    printf ("GRETH : driver has been attached\n");
#endif
    return 1;
}

/******************* Driver manager interface ***********************/

/* Driver prototypes */
int greth_register_io(rtems_device_major_number *m);
int greth_device_init(struct greth_softc *sc);
int network_interface_add(struct rtems_bsdnet_ifconfig *interface);

int greth_init2(struct rtems_drvmgr_dev_info *dev);
int greth_init3(struct rtems_drvmgr_dev_info *dev);

struct rtems_drvmgr_drv_ops greth_ops = 
{
	.init	=
		{
			NULL,
			greth_init2,
			greth_init3,
			NULL
		},
	.remove = NULL,
	.info = NULL
};

struct amba_dev_id greth_ids[] = 
{
	{VENDOR_GAISLER, GAISLER_ETHMAC},
	{0, 0}		/* Mark end of table */
};

struct amba_drv_info greth_drv_info =
{
	{
		NULL,				/* Next driver */
		NULL,				/* Device list */
		DRIVER_AMBAPP_GAISLER_GRETH_ID,	/* Driver ID */
		"GRETH_DRV",			/* Driver Name */
		DRVMGR_BUS_TYPE_AMBAPP,		/* Bus Type */
		&greth_ops,
		0,				/* No devices yet */
	},
	&greth_ids[0]
};

void greth_register_drv (void)
{
	DBG("Registering GRETH driver\n");
	rtems_drvmgr_drv_register(&greth_drv_info.general);
}

int greth_init2(struct rtems_drvmgr_dev_info *dev)
{
	struct greth_softc *priv;

	DBG("GRETH[%d] on bus %s\n", dev->minor_drv, dev->parent->dev->name);
	priv = dev->priv = malloc(sizeof(struct greth_softc));
	if ( !priv )
		return DRVMGR_NOMEM;
	memset(priv, 0, sizeof(*priv));
	priv->dev = dev;

	/* This core will not find other cores, so we wait for init3() */

	return DRVMGR_OK;
}

int greth_init3(struct rtems_drvmgr_dev_info *dev)
{
    struct greth_softc *sc;
    struct rtems_bsdnet_ifconfig *ifp;
    rtems_status_code status;

    sc = dev->priv;
    sprintf(sc->devName, "gr_eth%d", (dev->minor_drv+1));

    /* Init GRETH device */
    if ( greth_device_init(sc) ) {
        printk("GRETH: Failed to init device\n");
        return DRVMGR_FAIL;
    }

    /* Register GRETH device as an Network interface */
    ifp = malloc(sizeof(struct rtems_bsdnet_ifconfig));
    memset(ifp, 0, sizeof(*ifp));

    ifp->name = sc->devName;
    ifp->drv_ctrl = sc;
    ifp->attach = greth_interface_driver_attach;

    status = network_interface_add(ifp);
    if (status != 0) {
        return DRVMGR_FAIL;
    }

    return DRVMGR_OK;
}

int greth_device_init(struct greth_softc *sc)
{
    struct amba_dev_info *ambadev;
    struct ambapp_core *pnpinfo;
    union rtems_drvmgr_key_value *value;

    /* Get device information from AMBA PnP information */
    ambadev = (struct amba_dev_info *)sc->dev->businfo;
    if ( ambadev == NULL ) {
        return -1;
    }
    pnpinfo = &ambadev->info;
    sc->regs = (greth_regs *)pnpinfo->apb_slv->start;
    sc->minor = sc->dev->minor_drv;

    /* clear control register and reset NIC 
     * This should be done as quick as possible during startup, this is to
     * stop DMA transfers after a reboot.
     */
    sc->regs->ctrl = 0;
    sc->regs->ctrl = GRETH_CTRL_RST;
    sc->regs->ctrl = 0;

    /* Configure driver by overriding default config with the bus resources 
     * configured by the user
     */
    sc->txbufs = 32;
    sc->rxbufs = 32;
    sc->phyaddr = -1;

    value = rtems_drvmgr_dev_key_get(sc->dev, "txDescs", KEY_TYPE_INT);
    if ( value && (value->i <= 128) )
        sc->txbufs = value->i;

    value = rtems_drvmgr_dev_key_get(sc->dev, "rxDescs", KEY_TYPE_INT);
    if ( value && (value->i <= 128) )
        sc->rxbufs = value->i;

    value = rtems_drvmgr_dev_key_get(sc->dev, "phyAdr", KEY_TYPE_INT);
    if ( value && (value->i < 32) )
        sc->phyaddr = value->i;

    /* install interrupt handler */
    if ( rtems_drvmgr_interrupt_register(sc->dev, 0, greth_interrupt, sc) ) {
        printk("GRETH: Failed to register interrupt\n");
        return -1;
    }

    return 0;
}
