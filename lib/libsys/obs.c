//
// Created by Jan on 2/11/2018.
//
#include <sys/cdefs.h>
#define _SYSTEM	1
#define _MINIX 1

#include <sys/types.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/rs.h>
#include <sys/cdefs.h>
#include <lib.h>
#include <stdio.h>

int watch_exit(endpoint_t ep) {
    int status;// status_from_pm;
    message m, to_pm;
//    to_pm.m1_i1 = ep;
//    to_pm.m1_i2 = OBS_BY_ENDPOINT;
//    status_from_pm = _syscall(PM_PROC_NR, GET_ENDPT, &to_pm);
//    if( status_from_pm == -1 ) return -1;

    m.m_type = OBS_WATCH_EXIT;
    m.m1_i1 = ep;

    endpoint_t obs_address;
    if( minix_rs_lookup("obs", &obs_address) != OK ){
        printf("OBS NOT FOUND!\n");
        return -1;
    }
    status = sendrec(obs_address, &m);
    if (status != 0) {
        m.m_type = status;
    }
    if (m.m_type < 0) {
        errno = -m.m_type;
        return(-1);
    }
    return OK;
}

int cancel_watch_exit(endpoint_t ep){
    int status;
    message m;
    m.m_type = OBS_CANCEL_WATCH_EXIT;
    m.m1_i1 = ep;
    endpoint_t obs_address;
    if( minix_rs_lookup("obs", &obs_address) != OK ){
        printf("OBS NOT FOUND!\n");
        return -1;
    }
    status = sendrec(obs_address, &m);
    if (status != 0) {
        m.m_type = status;
    }
    if (m.m_type < 0) {
        errno = -m.m_type;
        return(-1);
    }
    return OK;
}

int query_exit(endpoint_t *ep){
    int status;
    message m;
    m.m_type = OBS_QUERY_EXIT;
    endpoint_t obs_address;
    if( minix_rs_lookup("obs", &obs_address) != OK ){
        printf("OBS NOT FOUND!\n");
        return -1;
    }
    status = sendrec(obs_address, &m);
    if (status != 0) {
        m.m_type = status;
    }
    if (m.m_type < 0) {
        errno = -m.m_type;
        return(-1);
    }
    else{
        *ep = m.m1_i1;
        return m.m1_i2;
    }
}