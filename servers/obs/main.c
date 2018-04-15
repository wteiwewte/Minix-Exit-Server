#include "inc.h"
#include "proto.h"
#define BY_PID 1;
#define BY_ENDPOINT 2;

int identifier = 0x1234;
endpoint_t who_e;
int call_type;
endpoint_t SELF_E;

static const int size_of_arrays = 1000;
static int verbose = 0;

struct watch_node{
    endpoint_t observator;
    endpoint_t who;
    int blocking;
};

struct watch_node_arr{
    struct watch_node arr[size_of_arrays];
    int current_len;
};

/*array for processes being watched which haven't died yet*/
struct watch_node_arr observe_list;
/*array for processes being watched which have died already*/
struct watch_node_arr query_list;

static struct {
	int type;
	int (*func)(message *);
	int reply;	/* whether the reply action is passed through */
    char * name;
} obs_calls[] = {
	{ OBS_CANCEL_WATCH_EXIT,	do_cancel_watch_exit,	0, "cancel_watch_exit" },
	{ OBS_WATCH_EXIT,	do_watch_exit,	0, "watch_exit" },
	{ OBS_PM_INFO,	pm_info,	1, "pm_info" },
	{ OBS_QUERY_EXIT,	do_query_exit,	0, "query_exit" },
	{ OBS_WAIT_EXIT,	do_wait_exit,	1, "wait_exit" },
};

#define SIZE(a) (sizeof(a)/sizeof(a[0]))

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

static void init_tables();

int main(int argc, char *argv[])
{
	message m;

    init_tables();
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		int r;
		int obs_number;

		if ((r = sef_receive(ANY, &m)) != OK)
			printf("sef_receive failed %d.\n", r);
		who_e = m.m_source;
		call_type = m.m_type;

        obs_number = call_type - OBS_BASE;
		if(verbose)
			printf("OBS: get %d (%s) from %d\n", call_type, obs_calls[obs_number].name, who_e);



		/* dispatch message */
		if (obs_number >= 0 && obs_number < SIZE(obs_calls)) {
			int result;

			if (obs_calls[obs_number].type != call_type)
				panic("OBS: call table order mismatch");

            /* if we are calling function available only to system processes*/
			if(call_type == OBS_CANCEL_WATCH_EXIT || call_type == OBS_WATCH_EXIT || call_type == OBS_QUERY_EXIT) {
                if (check_if_system_process(&m)) {
                    if( verbose ) printf("%d IS SYSTEM PROCESS\n", who_e);
                    result = obs_calls[obs_number].func(&m);
                } else {
                    if( verbose ) printf("%d IS NOT SYSTEM PROCESS\n", who_e);
                    result = EACCES;
                }
            }
            else {

                result = obs_calls[obs_number].func(&m);
            }
            //result = obs_calls[obs_number].func(&m);

			if (!obs_calls[obs_number].reply || result < 0 ) {

				m.m_type = result;

				if(verbose && result != OK)
					printf("OBS: error for %d: %d\n",
						call_type, result);

				if ((r = sendnb(who_e, &m)) != OK)
					printf("OBS send error %d.\n", r);
			}
		} else {
			/* warn and then ignore */
			printf("OBS unknown call type: %d from %d.\n",
				call_type, who_e);
		}
		update_refcount_and_destroy();
	}

	/* no way to get here */
	return -1;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the obs server. */

  SELF_E = getprocnr();

    printf("OBS: self: %d\n", SELF_E);

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* Checkout if there are still OBS keys. Inform the user in that case. */
  if (!is_sem_nil() || !is_shm_nil())
      printf("OBS: exit with un-clean states.\n");
}

/*===========================================================================*
 *		            OBS watch_table                     *
 *===========================================================================*/

static void init_tables(){
    /* function initiating our arrays, especially setting blocking to 0 */
    observe_list.current_len = 0;
    query_list.current_len = 0;
    int i;
    for(i = 0 ; i < size_of_arrays; ++i){
        observe_list.arr[i].blocking = 0;
        query_list.arr[i].blocking = 0;
    }

}

int in_table(struct watch_node_arr *tab, struct watch_node* to_check){
    /* function checking if array contains given node */
    int i;
    for(i = 0; i < tab->current_len; ++i ){
        if( tab->arr[i].observator == to_check->observator && tab->arr[i].who == to_check->who)
            return (TRUE);
    }
    return (FALSE);
}

int add(struct watch_node_arr* tab, struct watch_node* to_add){
    /* function adding node from array */
    if( tab->current_len == size_of_arrays ) {
        /* if there are no space in array return */
        return (-1);
    }
    tab->arr[tab->current_len].observator = to_add->observator;
    tab->arr[tab->current_len].who = to_add->who;
    tab->arr[tab->current_len].blocking = to_add->blocking;
    tab->current_len++;
    return (OK);
}

int del(struct watch_node_arr* tab, struct watch_node* to_del){
    /* function deleting node from array */
    if( tab->current_len == 0 ) {
        /* if there are no nodes in array return */
        return (-1);
    }
    int i;
    for(i = 0; i < tab->current_len; ++i ){
        if( tab->arr[i].observator == to_del->observator && tab->arr[i].who == to_del->who){
            tab->arr[i].observator = tab->arr[tab->current_len-1].observator;
            tab->arr[i].who = tab->arr[tab->current_len-1].who;
            tab->arr[i].blocking = tab->arr[tab->current_len-1].blocking;
            tab->current_len--;
            return(OK);
        }
    }
    return (ESRCH);
}

int in_table_watcher(struct watch_node_arr *tab, endpoint_t who_to_check){
    /* function to check if given process is observating someone */
    int i;
    for(i = 0; i < tab->current_len; ++i ){
        if( tab->arr[i].observator == who_to_check )
            return (TRUE);
    }
    return (FALSE);
}

/*===========================================================================*
 *		            Utility functions                          *
 *===========================================================================*/

int similar_syscall(endpoint_t who, int syscallnr, message *msgptr)
{
    int status;

    msgptr->m_type = syscallnr;
    status = sendrec(who, msgptr);
    if (status != 0) {
        if( verbose ) printf("Similar syscall sendrec failed!\n"); /* 'sendrec' itself failed. */

        msgptr->m_type = status;
    }
    if (msgptr->m_type < 0) {
        if( verbose ) printf("msgptr->m_type <0 similar syscall\n");
        errno = -msgptr->m_type;
        return(-1);
    }
    return(msgptr->m_type);
}

int check_if_system_process(message* m){
    struct priv my_priv;
    if(sys_getpriv(&my_priv, who_e) != OK ){
        printf("Sys_getprivtab failed!\n");
        return -1;
    }
    if (!(my_priv.s_flags & SYS_PROC)) {
        return 0;
    }
    return 1;
}

int info_pm_about_proc(endpoint_t ep){
    message to_pm;
    to_pm.m1_i1 = ep;
    /* mark our process so we will get info from pm after its death */
    if( similar_syscall(PM_PROC_NR, DO_OBS, &to_pm) != OK ) {
        if( verbose ) printf("info_pm_about_proc failed ");
        return -1;
    }
    return OK;

}

int pm_info(message* m){
    /* got info from pm */
    if( m->m1_i2 == OBS_DEAD_PROC ){
        /* info is about dead process which we have marked earlier */
        if( verbose ) printf("pm_info dead proc ");
        endpoint_t who = m->m1_i1;
        int i;
        for(i = 0; i < observe_list.current_len; ++i ){
            if(observe_list.arr[i].who == who){
                if(observe_list.arr[i].blocking) {
                    /* if observator is blocked we must send message to him (user process)*/
                    m->m_type = OK;
                    if (sendnb(observe_list.arr[i].observator, m) != OK)
                        if( verbose ) printf("Sending message didn't go well!\n");
                }
                else {
                    /* if observator is not blocked notify will be enough (system process)*/
                    if (notify(observe_list.arr[i].observator) != OK)
                        if( verbose ) printf("Sending notify didn't go well!\n");
                }
                /* adding query to our query list */
                add(&query_list, &observe_list.arr[i]);
                del(&observe_list, &observe_list.arr[i]);
                i--;
            }
            else if( observe_list.arr[i].observator == who ){
                /* we don't want to keep useless information in our array */
                del(&observe_list, &observe_list.arr[i]);
                i--;
            }

        }
    }
    else if( m->m1_i2 == OBS_SIGNAL_INTR ){
        /* we get info about signal to process */
        /* check if that process is observating someone, if not, return */
        if( !in_table_watcher(&observe_list, m->m1_i1) ) {

            return(OK);
        }
        /*
         * if yes, send message to process causing interrupt our syscall
         * and setting errno to EINTR
         * */
        m->m_type = EINTR;
        if( verbose ) printf("pm_info interrupted %d\n", m->m1_i3);
        if( sendnb(m->m1_i1, m) != OK)
            if( verbose ) printf("Sending message after interrupt didn't go well!\n");
    }
    return (OK);
}




/*===========================================================================*
 *		            OBS server logic                           *
 *===========================================================================*/




int do_watch_exit(message* m){
    /* sending message to PM to mark process who is being observated */
    if( info_pm_about_proc(m->m1_i1) == -1 ){
        return (-1);
    }

    struct watch_node tmp;
    tmp.observator = who_e;
    tmp.who = m->m1_i1;
    /*for system processes we don't want to block them*/
    tmp.blocking = 0;

    if( in_table(&observe_list, &tmp) == TRUE ){
        /* check if we don't have it already */
        printf("Process already in our table!\n");
        return (EAGAIN);
    }
    add(&observe_list, &tmp);
	return (OK);
}

int do_cancel_watch_exit(message *m){
    /* cancel_watch_exit just tries to perform delete on our array */
    struct watch_node tmp;
    tmp.observator = who_e;
    tmp.who = m->m1_i1;
    return del(&observe_list, &tmp);
}

int do_query_exit(message* m){
    endpoint_t to_send;
    int number_of_nonreturned_processes = 0;
    int i;
    for(i = 0 ; i < query_list.current_len; ++i ){
        if(query_list.arr[i].observator == who_e){
            /* if observator is sender of message, increment counter */
            if( number_of_nonreturned_processes == 0 ){
                /* if we didn't set endpoint to reply, let's set it */
                to_send = query_list.arr[i].who;
                del(&query_list, &query_list.arr[i]);
                i--;
            }
            number_of_nonreturned_processes++;
        }
    }
    m->m1_i1 = to_send;
    m->m1_i2 = number_of_nonreturned_processes-1;
    if( number_of_nonreturned_processes == 0){
        /* there is no dead processes which had been watched by calling process */
        return (-1);
    }
    else return (OK);
}



int do_wait_exit(message *m) {
    /*
     * we want to mark process who is being observated
     * */
    if( info_pm_about_proc(m->m1_i1) == -1 ){
        return (-1);
    }
    endpoint_t endpt = m->m1_i1;

    m->m1_i1 = who_e;
    /*
     * we want to mark process who is observating in case of getting signal and thus interrupted
     * */
    if( info_pm_about_proc(m->m1_i1) == -1 ){
        return (-1);
    }
    /* adding new node to our array */
    struct watch_node tmp;
    tmp.observator = who_e;
    tmp.who = endpt;
    /* for user process we want to block it */
    tmp.blocking = 1;

    add(&observe_list, &tmp);
	return (OK);


}
