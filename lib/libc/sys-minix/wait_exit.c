//
// Created by Jan on 2/1/2018.
//
#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <stdio.h>
#include <minix/rs.h>
#include <minix/com.h>
#ifdef __weak_alias
__weak_alias(unlink, _unlink)
#endif

#define OK 0

int wait_exit(pid_t p){
    endpoint_t OBS_NR_PROC;
    int r, status, status_from_pm;
    message m, to_pm;
    to_pm.m1_i1 = p;
    to_pm.m1_i2 = OBS_BY_PID;
    status_from_pm = _syscall(PM_PROC_NR, GET_ENDPT, &to_pm);
    if( status_from_pm == -1 ) return -1;
    if((r = minix_rs_lookup("obs", &OBS_NR_PROC)) == OK){
        m.m1_i1 = to_pm.m1_i1;
        status = (_syscall(OBS_NR_PROC, OBS_WAIT_EXIT, &m));
    }
    else {
    	printf("CANNOT GET ENDPOINT!\n");
    }
    return status;
}
