//
// Created by Jan on 2/1/2018.
//
#ifndef MINIX_EDITED_PROTO_H
#define MINIX_EDITED_PROTO_H
int do_cancel_watch_exit(message* m);
int do_watch_exit(message* m);
int pm_info(message* m);
int do_query_exit(message* m);
int do_wait_exit(message* m);


/* obs_utility */
int check_if_system_process(message* m);
int similar_syscall(endpoint_t who, int syscallnr, message *msgptr);
int info_pm_about_proc(endpoint_t ep);
int pm_info(message* m);



#endif //MINIX_EDITED_PROTO_H
