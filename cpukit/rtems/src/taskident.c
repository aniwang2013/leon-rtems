/*
 *  RTEMS Task Manager
 *
 *
 *  COPYRIGHT (c) 1989-1999.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id$
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/system.h>
#include <rtems/rtems/status.h>
#include <rtems/rtems/support.h>
#include <rtems/rtems/modes.h>
#include <rtems/score/object.h>
#include <rtems/score/stack.h>
#include <rtems/score/states.h>
#include <rtems/rtems/tasks.h>
#include <rtems/score/thread.h>
#include <rtems/score/threadq.h>
#include <rtems/score/tod.h>
#include <rtems/score/userext.h>
#include <rtems/score/wkspace.h>
#include <rtems/score/apiext.h>
#include <rtems/score/sysstate.h>

/*
 *  rtems_task_ident
 *
 *  This directive returns the system ID associated with
 *  the thread name.
 *
 *  Input parameters:
 *    name - user defined thread name
 *    node - node(s) to be searched
 *    id   - pointer to thread id
 *
 *  Output parameters:
 *    *id               - thread id
 *    RTEMS_SUCCESSFUL - if successful
 *    error code        - if unsuccessful
 */

rtems_status_code rtems_task_ident(
  rtems_name    name,
  uint32_t      node,
  rtems_id     *id
)
{
  Objects_Name_or_id_lookup_errors  status;

  if ( !id )
    return RTEMS_INVALID_ADDRESS;

  if ( name == OBJECTS_ID_OF_SELF ) {
    *id = _Thread_Executing->Object.id;
    return RTEMS_SUCCESSFUL;
   }

  status = _Objects_Name_to_id_u32( &_RTEMS_tasks_Information, name, node, id );

  return _Status_Object_name_errors_to_status[ status ];
}
