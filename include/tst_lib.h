//
// Created by Jan on 2/11/2018.
//

#ifndef MINIX_EDITED_TST_LIB_H
#define MINIX_EDITED_TST_LIB_H
#include <minix/rs.h>

#define OK 0

#define DATA m1_i1

#define TST_RETRIEVE 	1
#define TST_RETRIEVE_NB	2
#define TST_STORE	3
#define TST_STORE_NB	4

char * server_name="tst";

static int get_tst_endpt(endpoint_t * ep){
    return minix_rs_lookup(server_name, ep);
}

void update_server_name(int argc, char * argv[]){
    if (argc==2){
        int res;
        endpoint_t dummy;
        server_name=argv[1];
        res= get_tst_endpt(&dummy);
        if (res!=OK){
            fprintf(stderr,"Server lookup failed.\n");
            exit(1);
        }
    }
}

int store(int i){
    endpoint_t srv;
    message m;
    m.DATA = i;
    if (get_tst_endpt(&srv) != OK){
        errno = ENOSYS;
        return -1;
    };

    return _syscall(srv, TST_STORE, &m);
}

int store_nb(int i){
    endpoint_t srv;
    message m;
    m.DATA = i;
    if (get_tst_endpt(&srv) != OK){
        errno = ENOSYS;
        return -1;
    };

    return _syscall(srv, TST_STORE_NB, &m);
}


int retrieve(int *i){
    int res;
    endpoint_t srv;
    message m;
    if (get_tst_endpt(&srv) != OK){
        errno = ENOSYS;
        return -1;
    };

    res = _syscall(srv, TST_RETRIEVE, &m);
    if (res != OK) return res;
    *i = m.DATA;
    return res;
}

int retrieve_nb(int *i){
    int res;
    endpoint_t srv;
    message m;
    if (get_tst_endpt(&srv) != OK){
        errno = ENOSYS;
        return -1;
    };

    res = _syscall(srv, TST_RETRIEVE_NB, &m);
    if (res != OK) return res;
    *i = m.DATA;
    return res;
}

#endif //MINIX_EDITED_TST_LIB_H
