#include "inc.h"

#include "tst.h"

#define NOSERV -1
#define FAIL -1


endpoint_t who_e;
endpoint_t SELF_E;

int storage;
int is_empty=1;
endpoint_t storer;

int do_retrieve(message *);
int do_retrieve_nb(message *);
static int do_store_common(endpoint_t source, int data, int start_watching);
int do_store(message *);
int do_store_nb(message *);
int proc_exited(endpoint_t ep);


static struct {
	int type;
	int (*func)(message *);
	int reply;	/* whether the reply action is passed through (delegated to func) */
	char * name;
} call_vec[] = {
	{ TST_RETRIEVE,	do_retrieve,	0, "retrieve"},
	{ TST_RETRIEVE_NB,	do_retrieve_nb,	0, "retrieve_nb"},
	{ TST_STORE,	do_store,	0, "store" },
	{ TST_STORE_NB,	do_store_nb,	0, "store_nb" },
};

#define SIZE(a) (sizeof(a)/sizeof(a[0]))

static const int verbose =0 ;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

void query_exits_loop(void);

int main(int argc, char *argv[])
{
	message m;
	int who_p;
	int call_type;
	int r;
	int result;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		int found;
		int call_entry_i;

		if ((r = sef_receive(ANY, &m)) != OK)
			printf("TST: sef_receive failed %d.\n", r);
		who_e = m.m_source;
		who_p = _ENDPOINT_P(who_e);
		call_type = m.m_type;


		if (call_type & NOTIFY_MESSAGE) {
			query_exits_loop();
			continue;

		}

		// browsing call_vec
		found = 0;
		for (call_entry_i=0; call_entry_i< SIZE(call_vec); call_entry_i++){
			if (call_vec[call_entry_i].type != call_type) continue;
			
			//entry found
			if(verbose)
				printf("TST: got %d (%s) from %d\n", call_type, call_vec[call_entry_i].name, who_e);
			found=1;	
			result = call_vec[call_entry_i].func(&m);
			break;
		}

		if (!found){
			printf("TST: invalid message from %d, unknown type %d. Ignoring...\n", who_e, call_type);
			continue;
		}


		if ( (!call_vec[call_entry_i].reply) && (result != SUSPEND) ) {

			m.m_type = result;

			if(verbose && result != OK)
				printf("TST: error for %d (%s): %d\n",
					call_type, call_vec[call_entry_i].name, result);

			if ((r = sendnb(who_e, &m)) != OK)
				printf("TST: send error %d.\n", r);
		}
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
/* Initialize the tst server. */

  SELF_E = getprocnr();

      printf("TST: I'm alive. Self: %d\n", SELF_E);

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  if (verbose){
	  printf("TST: I quit.\n");
  }
}

/*===========================================================================*
 *		            query_exits_loop	                             *
 *===========================================================================*/
void query_exits_loop(void){
    //printf("QUERY_EXIT\n");
    int count, res;
	endpoint_t ep;

	do {

		res = query_exit(&ep);
		if (res < 0){
			if (verbose){ printf("TST: query exit failed with %d. ", res);}
			return;
		}

		count= res;
		if (verbose){
			printf("TST: proc %d exited (remaining count: %d). ", ep, count);
		}
		proc_exited(ep);
	} while (count > 0);
}

/*===========================================================================*
 *		            Implementaion of wait queues                     *
 *===========================================================================*/

#define QUEUE_BUF_SIZE NR_PROCS
/* we never queue ourselves so the queues shall never be full */
struct message_queue {
	message buf[QUEUE_BUF_SIZE];
	int first;
	int after_last;
};

struct message_queue storers;
struct message_queue retrievers;

inline int qinc(int i){
	return (i+1)%QUEUE_BUF_SIZE;
}

inline int qdec(int i){
	return (((i-1)%QUEUE_BUF_SIZE)+QUEUE_BUF_SIZE)%QUEUE_BUF_SIZE;
}

int enqueue(struct message_queue * mq, message * m){
	if (qinc(mq->after_last)  == mq->first){
		printf("TST: queue full.\n");
		return ENOSPC;
	}
	
	mq->buf[mq->after_last]=*m;
	mq->after_last = qinc(mq->after_last);
	return OK;
}

/* dequeue returns pointer to a message in buffer, it is considered free for the queue so has to be used immediately or not at all*/
int dequeue(struct message_queue * mq, message *m){
	if (mq->after_last == mq->first) return ENODATA;

	*m = mq->buf[mq->first];
	mq->first = qinc(mq->first);
	return OK;
}

/* removes once */
int remove_from_queue(struct message_queue * mq, endpoint_t ep){
	int i;

	for (i = mq->first; i != mq->after_last; i = qinc(i)){
		if (mq->buf[i].m_source == ep) break;
	}
	if (i == mq->after_last) return FAIL;

	for (; qinc(i) != mq->after_last; i = qinc(i)){
		mq->buf[i] = mq->buf[qinc(i)];
	}
	mq->after_last = qdec(mq->after_last);
	return OK;
}

int in_queue(struct message_queue * mq, endpoint_t ep){
	int i;

	for (i = mq->first; i != mq->after_last; i = qinc(i)){
		if (mq->buf[i].m_source == ep) return TRUE;
	}
	return FALSE;
}

/*===========================================================================*
 *		            Server logic		                     *
 *===========================================================================*/

int do_retrieve(message * m){
	if (!is_empty) return do_retrieve_nb(m);

	// else save and suspend
	if (verbose){
		printf("TST: Registering watch_exit for %d. ", m->m_source);
	}
	if (watch_exit(m->m_source)!= OK){
		if (verbose) printf("TST: watchexit failed with errno: %d, ", errno);
		return errno;
	};
	enqueue(&retrievers, m);
	return SUSPEND;
};

int do_retrieve_nb(message * m){
	if (is_empty) return ENODATA;
	m->DATA = storage;
	is_empty = TRUE;

	// stop watching curent storer if not in queues 
	if ( (storer != NOSERV) 	// equal when called from proc_exited
		&& !in_queue(&storers, storer) && !in_queue(&retrievers, storer)){
		int result = cancel_watch_exit(storer);
		if (verbose && result != OK){
			printf("TST: cancelwatch for %d failed with %d. ",  storer, errno);
		}
	}
	storer = NOSERV;

	// wake up next storer
	message response;
	if (dequeue(&storers, &response) == OK){
		int result;
		result = do_store_common(response.m_source, response.DATA ,FALSE);

		response.m_type = result;

		if(verbose && result != OK){
			printf("TST: error for store: %d\n",  result);
		}
		if ((result = sendnb(response.m_source, &response)) != OK){
			printf("TST: revive send to %d failed with %d. ", response.m_source, result);
		}
	}

	return OK;
}

static int do_store_common(endpoint_t source, int data, int start_watching){
	if (!is_empty) return ENOSPC;

	storage = data;
	is_empty = FALSE;
	storer = source;
	if (start_watching){
		if (verbose) printf("TST: Registering watch_exit for %d. ", source);
  		if (watch_exit(storer)!= OK){
			printf("TST: watchexit failed with errno: %d, ", errno);
			return -errno;
		}
	};
	// wake retriever
	message response;
	if (dequeue(&retrievers, &response)==OK){
		int result;
		result = do_retrieve_nb(&response);

		response.m_type = result;

		cancel_watch_exit(response.m_source);
		if(verbose && result != OK){
			printf("TST: error for retrieve: %d ", result);
		}
		if ((result = sendnb(response.m_source, &response)) != OK){
			printf("TST: revive send error %d. ", result);
		}
	}
	return OK;
}

int do_store(message * m){
	if (is_empty) return do_store_nb(m);

	//else save and suspend
	if (verbose) printf("TST: Registering watch_exit for %d. ", m->m_source);
	//check if sender is watched, all procs in queues are blocked
	//the only watched running proc is the current storer
	if (storer != m->m_source){	
		if (watch_exit(m->m_source)!= OK){
			printf("TST: watchexit failed with errno: %d, ", errno);
			return errno;
		}
	};
	enqueue(&storers, m);
	return SUSPEND;
}

int do_store_nb(message * m){
	return do_store_common(m->m_source, m->DATA, TRUE);
}

int proc_exited(endpoint_t ep){
	int r1, r2;

	if (verbose){
		printf("TST: removing %d from queues.\n", ep);
	}

	r1 = remove_from_queue(&storers, ep);
	r2 = remove_from_queue(&retrievers, ep);
	if (storer==ep) {
		message dummy;
		if (verbose) printf("TST: removing exited storer.\n");
		storer = NOSERV;
		do_retrieve_nb(&dummy); // wakes the next storer
	} 
	else if ((r1==FAIL) && (r2==FAIL)){
		printf("TST: exited proces %d not found in queues and is not the storer.\n", ep);
	}
	return OK;
}
