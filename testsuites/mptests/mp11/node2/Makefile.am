##
## $Id$
##

rtems_tests_PROGRAMS = mp11-node2
mp11_node2_SOURCES = ../init.c ../system.h

dist_rtems_tests_DATA = mp11-node2.scn
dist_rtems_tests_DATA += mp11-node2.doc

include $(RTEMS_ROOT)/make/custom/@RTEMS_BSP@.cfg
include $(top_srcdir)/../automake/compile.am
include $(top_srcdir)/../automake/leaf.am

AM_CPPFLAGS += -DNODE_NUMBER=2 -I$(srcdir)/..

AM_CPPFLAGS += -I$(top_srcdir)/../support/include

LINK_OBJS = 
LINK_LIBS = $(mp11_node2_LDLIBS)

mp11-node2$(EXEEXT): $(mp11_node2_OBJECTS) $(mp11_node2_DEPENDENCIES)
	@rm -f mp11-node2$(EXEEXT)
	$(make-exe)

include $(top_srcdir)/../automake/local.am
