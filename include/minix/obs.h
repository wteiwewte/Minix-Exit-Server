//
// Created by Jan on 2/11/2018.
//

#ifndef MINIX_EDITED_OBS_H
#define MINIX_EDITED_OBS_H
#include <sys/types.h>
int wait_exit(pid_t);

int watch_exit(endpoint_t ep);
int cancel_watch_exit(endpoint_t ep);
int query_exit(endpoint_t *ep);

#endif //MINIX_EDITED_OBS_H
