#
#  Config file for a PowerPC MPC8xx-based TQM8xx carrier card
#

include $(RTEMS_ROOT)/make/custom/default.cfg

RTEMS_CPU=powerpc
RTEMS_CPU_MODEL=mpc860

# This is the actual bsp directory used during the build process.
RTEMS_BSP_FAMILY=tqm8xx

CPU_CFLAGS = -mcpu=860 -Dmpc860 \
             -mstrict-align -fno-strict-aliasing \
             -meabi -msdata -fno-common

# optimize flag: typically -O2
CFLAGS_OPTIMIZE_V = -O2 -g -fno-keep-inline-functions

RTEMS_LINKCMDS=linkcmds.tqm8xx
LDFLAGS += -qnolinkcmds -T $(RTEMS_LINKCMDS)

define bsp-post-link
	cp $(basename $@).exe $(basename $@)$(DOWNEXT) 
	$(STRIP) $(basename $@)$(DOWNEXT)
	$(default-bsp-post-link)
endef