/*
 * imfs_fifo.c: FIFO support for IMFS
 *
 * Author: Wei Shen <cquark@gmail.com>
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rtems.com/license/LICENSE.
 *
 * $Id$
 */

#if HAVE_CONFIG_H
  #include "config.h"
#endif

#include "imfs.h"

#define JNODE2PIPE(_jnode)  ( (_jnode)->info.fifo.pipe )

#define LIBIO2PIPE(_iop)  ( JNODE2PIPE((IMFS_jnode_t *)(_iop)->pathinfo.node_access) )

/* Set errno and return -1 if error, else return _err */
#define IMFS_FIFO_RETURN(_err) \
do {  \
  if (_err < 0)	\
    rtems_set_errno_and_return_minus_one(-_err); \
  return _err; \
} while (0)

static int IMFS_fifo_open(
  rtems_libio_t *iop,
  const char    *pathname,
  int            oflag,
  mode_t         mode
)
{
  IMFS_jnode_t *jnode = iop->pathinfo.node_access;

  int err = fifo_open(&JNODE2PIPE(jnode), iop);
  IMFS_FIFO_RETURN(err);
}

static int IMFS_fifo_close(
  rtems_libio_t *iop
)
{
  int err = 0;
  IMFS_jnode_t *jnode = iop->pathinfo.node_access;

  pipe_release(&JNODE2PIPE(jnode), iop);

  IMFS_FIFO_RETURN(err);
}

static ssize_t IMFS_fifo_read(
  rtems_libio_t *iop,
  void          *buffer,
  size_t         count
)
{
  IMFS_jnode_t *jnode = iop->pathinfo.node_access;

  int err = pipe_read(JNODE2PIPE(jnode), buffer, count, iop);
  if (err > 0)
    IMFS_update_atime(jnode);

  IMFS_FIFO_RETURN(err);
}

static ssize_t IMFS_fifo_write(
  rtems_libio_t *iop,
  const void    *buffer,
  size_t         count
)
{
  IMFS_jnode_t *jnode = iop->pathinfo.node_access;

  int err = pipe_write(JNODE2PIPE(jnode), buffer, count, iop);
  if (err > 0) {
    IMFS_mtime_ctime_update(jnode);
  }

  IMFS_FIFO_RETURN(err);
}

static int IMFS_fifo_ioctl(
  rtems_libio_t *iop,
  uint32_t       command,
  void          *buffer
)
{
  int err;

  if (command == FIONBIO) {
    if (buffer == NULL)
      err = -EFAULT;
    else {
      if (*(int *)buffer)
        iop->flags |= LIBIO_FLAGS_NO_DELAY;
      else
        iop->flags &= ~LIBIO_FLAGS_NO_DELAY;
      return 0;
    }
  }
  else
    err = pipe_ioctl(LIBIO2PIPE(iop), command, buffer, iop);

  IMFS_FIFO_RETURN(err);
}

static off_t IMFS_fifo_lseek(
  rtems_libio_t *iop,
  off_t          offset,
  int            whence
)
{
  off_t err = pipe_lseek(LIBIO2PIPE(iop), offset, whence, iop);
  IMFS_FIFO_RETURN(err);
}

static const rtems_filesystem_file_handlers_r IMFS_fifo_handlers = {
  IMFS_fifo_open,
  IMFS_fifo_close,
  IMFS_fifo_read,
  IMFS_fifo_write,
  IMFS_fifo_ioctl,
  IMFS_fifo_lseek,
  IMFS_stat,
  rtems_filesystem_default_ftruncate,
  rtems_filesystem_default_fsync_or_fdatasync,
  rtems_filesystem_default_fsync_or_fdatasync,
  rtems_filesystem_default_fcntl
};

const IMFS_node_control IMFS_node_control_fifo = {
  .imfs_type = IMFS_FIFO,
  .handlers = &IMFS_fifo_handlers,
  .node_initialize = IMFS_node_initialize_default,
  .node_remove = IMFS_node_remove_default,
  .node_destroy = IMFS_node_destroy_default
};
