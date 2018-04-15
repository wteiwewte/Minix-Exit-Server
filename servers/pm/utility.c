/* This file contains some utility routines for PM.
 *
 * The entry points are:
 *   get_free_pid:	get a free process or group id
 *   no_sys:		called for invalid system call numbers
 *   find_param:	look up a boot monitor parameter
 *   find_proc:		return process pointer from pid number
 *   nice_to_priority	convert nice level to priority queue
 *   pm_isokendpt:	check the validity of an endpoint
 *   tell_vfs:		send a request to VFS on behalf of a process
 */

#include "pm.h"
#include <sys/resource.h>
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <fcntl.h>
#include <signal.h>		/* needed only because mproc.h needs it */
#include "mproc.h"
#include "param.h"

#include <minix/config.h>
#include <timers.h>
#include <string.h>
#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"
#include "kernel/proc.h"
#include <minix/rs.h>
/*===========================================================================*
 *				get_free_pid				     *
 *===========================================================================*/
pid_t get_free_pid()
{
  static pid_t next_pid = INIT_PID + 1;		/* next pid to be assigned */
  register struct mproc *rmp;			/* check process table */
  int t;					/* zero if pid still free */

  /* Find a free pid for the child and put it in the table. */
  do {
	t = 0;			
	next_pid = (next_pid < NR_PIDS ? next_pid + 1 : INIT_PID + 1);
	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
		if (rmp->mp_pid == next_pid || rmp->mp_procgrp == next_pid) {
			t = 1;
			break;
		}
  } while (t);					/* 't' = 0 means pid free */
  return(next_pid);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys()
{
/* A system call number not implemented by PM has been requested. */
  return(ENOSYS);
}

/*===========================================================================*
 *				find_param				     *
 *===========================================================================*/
char *find_param(name)
const char *name;
{
  register const char *namep;
  register char *envp;

  for (envp = (char *) monitor_params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') 
		return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NULL);
}

/*===========================================================================*
 *				find_proc  				     *
 *===========================================================================*/
struct mproc *find_proc(lpid)
pid_t lpid;
{
  register struct mproc *rmp;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
	if ((rmp->mp_flags & IN_USE) && rmp->mp_pid == lpid)
		return(rmp);

  return(NULL);
}

/*===========================================================================*
 *				find_proc_by_endpt  				     *
 *===========================================================================*/
struct mproc *find_proc_by_endpt(lendpt)
endpoint_t lendpt;
{
  register struct mproc *rmp;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
	if ((rmp->mp_flags & IN_USE) && rmp->mp_endpoint == lendpt)
		return(rmp);

  return(NULL);
}

/*===========================================================================*
 *				nice_to_priority			     *
 *===========================================================================*/
int nice_to_priority(int nice, unsigned* new_q)
{
	if (nice < PRIO_MIN || nice > PRIO_MAX) return(EINVAL);

	*new_q = MAX_USER_Q + (nice-PRIO_MIN) * (MIN_USER_Q-MAX_USER_Q+1) /
	    (PRIO_MAX-PRIO_MIN+1);

	/* Neither of these should ever happen. */
	if ((signed) *new_q < MAX_USER_Q) *new_q = MAX_USER_Q;
	if (*new_q > MIN_USER_Q) *new_q = MIN_USER_Q;

	return (OK);
}

/*===========================================================================*
 *				pm_isokendpt			 	     *
 *===========================================================================*/
int pm_isokendpt(int endpoint, int *proc)
{
	*proc = _ENDPOINT_P(endpoint);
	if(*proc < -NR_TASKS || *proc >= NR_PROCS)
		return EINVAL;
	if(*proc >= 0 && endpoint != mproc[*proc].mp_endpoint)
		return EDEADEPT;
	if(*proc >= 0 && !(mproc[*proc].mp_flags & IN_USE))
		return EDEADEPT;
	return OK;
}

/*===========================================================================*
 *				tell_vfs			 	     *
 *===========================================================================*/
void tell_vfs(rmp, m_ptr)
struct mproc *rmp;
message *m_ptr;
{
/* Send a request to VFS, without blocking.
 */
  int r;

  if (rmp->mp_flags & VFS_CALL)
	panic("tell_vfs: not idle: %d", m_ptr->m_type);

  r = asynsend3(VFS_PROC_NR, m_ptr, AMF_NOREPLY);
  if (r != OK)
  	panic("unable to send to VFS: %d", r);

  rmp->mp_flags |= VFS_CALL;
}

/*===========================================================================*
 *				tell_obs			 	     *
 *===========================================================================*/
void tell_obs(rmp, m_ptr)
        struct mproc *rmp;
        message *m_ptr;
{
/* Send message to OBS telling about dead process (if this process may be observated by someone */
    if( rmp->mp_flags & OBS_DEAD ){
        message m_to_obs;
        m_to_obs.m1_i1 = rmp->mp_endpoint;
        m_to_obs.m1_i2 = OBS_DEAD_PROC;
        m_to_obs.m_type = OBS_PM_INFO;
        endpoint_t OBS_NR_PROC;
        if( minix_rs_lookup("obs", &OBS_NR_PROC) == OK ){
            if( asynsend3(OBS_NR_PROC, &m_to_obs, AMF_NOREPLY)!= OK ) {
                printf("Sending message to OBS about dead process didn't execute well!\n");
            }
        }
        else{
            printf("NO OBS PROC NR!\n");
        }

    }
}
/*===========================================================================*
 *				tell_obs_signal			 	     *
 *===========================================================================*/
void tell_obs_signal(rmp, m_ptr)
        struct mproc *rmp;
        message *m_ptr;
{
/* Send message to OBS telling about interrupted process */
    if( rmp->mp_flags & OBS_DEAD ){
        message m_to_obs;
        m_to_obs.m1_i1 = rmp->mp_endpoint;
        m_to_obs.m1_i2 = OBS_SIGNAL_INTR;
        m_to_obs.m1_i3 = rmp->mp_pid;
        m_to_obs.m_type = OBS_PM_INFO;
        endpoint_t OBS_NR_PROC;
        if( minix_rs_lookup("obs", &OBS_NR_PROC) == OK ){
            if( asynsend3(OBS_NR_PROC, &m_to_obs, AMF_NOREPLY) != OK ) {
                printf("Sending message to OBS about interrupted process by signal, didn't go well!\n");
            }
        }
        else{
            printf("NO OBS PROC NR!\n");
        }

    }
}
/*===========================================================================*
 *				do_obs			 	     *
 *===========================================================================*/
int do_obs()
{
    /* setting our OBS_DEAD flag telling which process may be observated by someone or may observate someone */
    endpoint_t who = m_in.m1_i1;
    register struct mproc *rmp;
    for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
        if ((rmp->mp_flags & IN_USE) && (rmp->mp_endpoint == who) && (!(rmp->mp_flags & EXITING ))) {
            rmp->mp_flags |= OBS_DEAD;
            return (OK);
        }
    }
    /*there is no such process*/
    return (ENOENT);
}

/*===========================================================================*
 *				get_endpt			 	     *
 *===========================================================================*/
int get_endpt(){
    register struct mproc* rmp;
    if( m_in.m1_i2 == OBS_BY_PID ){
        /* getting  proces query by pid */
        rmp = find_proc(m_in.m1_i1);
    }
    else if( m_in.m1_i2 == OBS_BY_ENDPOINT ){
        /* getting  proces query by endpoint */
        rmp = find_proc_by_endpt(m_in.m1_i1);
    }
    if( rmp == NULL ) {
        return (ESRCH);
    }
    else {
        mp->mp_reply.m1_i1 = rmp->mp_endpoint;
        if( rmp->mp_flags & PRIV_PROC ){
            mp->mp_reply.m1_i2 = 1;
        } else mp->mp_reply.m1_i2 = 0;
        return (OK);
    }
}