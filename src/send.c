#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "send.h"
#include "log.h"
#include "smt_ev.h"
#include "sqlite_db.h"

#define MAX_SIZE     200



//#define  SEND_LOCK(p) (pthread_mutex_lock(&(p)->lock))
//#define  SEND_UNLOCK(p) (pthread_mutex_unlock(&(p)->lock))

#define  SEND_LOCK(p)
#define  SEND_UNLOCK(p)

typedef struct send_data_s {
    smt_ev_loop loop;
    smt_timer_events timer_events;
    pthread_mutex_t lock;

} send_data_t;




static send_data_t * psend = NULL;



static int timer_cb(smt_ev_loop * ploop, void * arg)
{
    log_error(LOG_NOTICE, "buffer send thread running");
    return 0;
}

int send_init(void)
{


    if (psend != NULL)
        return 0;

    psend = malloc(sizeof(*psend));

    if (psend == NULL) {
        log_error(LOG_EMERG, "send_init");
        return -1;
    }

    memset(psend, 0 , sizeof(*psend));
    if (smt_init_eventloop(&psend->loop, 256, 10) < 0) {
        free(psend);
        log_error(LOG_EMERG, "send_init");
        return -1;
    }
    pthread_mutex_init(&psend->lock, NULL);


    timer_event_init(&psend->timer_events);

    timer_event_set(&psend->timer_events, 500000, timer_cb, NULL, NULL, SMT_TIMEREPEAT);

    if (smt_add_tevents(&psend->loop, &psend->timer_events) < 0) {
        log_error(LOG_ERROR, "smt_add_tevents");
        smt_destroy_event_loop(&psend->loop);
        free(psend);
        return -1;
    }


    return 0;

}

void send_destroy(void)
{
    smt_del_tevents(&psend->loop, &psend->timer_events);
    smt_destroy_event_loop(&psend->loop);
    pthread_mutex_destroy(&psend->lock);
    free(psend);
    psend = NULL;
}




int send_register(int fd, void * arg, smtFileProc proc, smt_fd_events * pevents)
{
    send_data_t * p = psend;

    SEND_LOCK(p);

    if (fd <= 0)
        return -1;




    fd_event_set(pevents, SMT_WRITABLE, fd, proc, arg);

    if (smt_add_fdevents(&p->loop, pevents) < 0) {
        log_error(LOG_EMERG, "send_register");
        SEND_UNLOCK(p);
        return -1;
    }

    SEND_UNLOCK(p);
    return 0;
}

void send_unregister(smt_fd_events * pevents)
{
    send_data_t * p = psend;
    SEND_LOCK(p);

    smt_del_fdevents(&p->loop, pevents);
    fd_event_init(pevents);
    SEND_UNLOCK(p);
}


void send_loop(void)
{
    send_data_t * p = psend;

    log_error(LOG_NOTICE, "send thread start");

    smt_event_loop(&p->loop);


}




