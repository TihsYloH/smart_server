
#ifndef SMT_EV_H_
#define SMT_EV_H_
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "min_heap.h"


#define HAVE_EPOLL

#define     SMT_MAX_EVENT_SZIE      50

#define     SMT_MAX_TIMER_SZIE      500

     
typedef struct smt_event_loop    smt_ev_loop;
typedef struct tm_event  smt_timer_events;
typedef struct fd_events         smt_fd_events;


typedef void (*smtFileProc)(struct smt_event_loop *eventLoop, void * clientData, int mask);
typedef int (*smtTimeProc)(struct smt_event_loop *eventLoop, void *clientData);
typedef void (* smtEventFinalizerProc)(struct smt_event_loop *eventLoop, void *clientData);




#define SMT_ERR     -1
#define SMT_OK      0

#define SMT_NONE 0

/*SMT_READABLE and SMT_WRITABLE  must choose one*/
#define SMT_READABLE 1
#define SMT_WRITABLE 2
#define SMT_POLLPRI  32

#define SMT_TIMERONECE   8
#define SMT_TIMEREPEAT  16
#define SMT_TIMERNONE   128   


#define      smt_getevent_mask(event)   (event->mask)




struct tm_event {
    struct timespec expires;
    struct timespec disp;
    //struct timeval expires;
    //struct timeval disp;
    smtTimeProc timeProc;
    smtEventFinalizerProc finalizerProc;
    void *arg;
    int mask;
    unsigned char isactive;
};


struct fd_events {
   int fd;
   int mask;
   smtFileProc  proc;
   void * arg;
   unsigned char active;
} ;



struct smt_event_loop {
   int fd_cnt;
   min_heap_t * heap;      //timer event heap

   int fd_size;
   int timer_size;

   struct timespec lasttm;

   void * poll_data;

   unsigned char stop;
} ;

/*start to loop*/
int smt_event_loop(struct smt_event_loop * loop);  
/*stop looping*/
int smt_loop_stop(struct smt_event_loop * loop);
/* single use or...  change the ms and mask*/
int smt_mod_tevents(struct smt_event_loop * loop,  smt_timer_events * tm, int ms, int mask);
/*delete timer*/
int smt_del_tevents(struct smt_event_loop * loop, smt_timer_events * tm);

/*add timer*/
int smt_add_tevents(struct smt_event_loop * loop, smt_timer_events * tm);

/*only init once or after smt_del_tevents*/
int timer_event_init(smt_timer_events * tm);


/*init timer follows smt_add_tevents or smt_mod_tevents*/
int timer_event_set(smt_timer_events * tm, int ms,
          smtTimeProc timeProc, smtEventFinalizerProc finalizerProc, void * arg, int mask);

int smt_del_fdevents(struct smt_event_loop * loop, struct fd_events * events);
/*       init an fd_event  can only be done once      */
int fd_event_init(struct fd_events * events );




/*set an fd event*/
int fd_event_set(struct fd_events * events, int mask, int fd, smtFileProc  proc, void * arg );
/*add fd events*/
int smt_add_fdevents(struct smt_event_loop * loop, struct fd_events * events);

/*init an event loop*/
int smt_init_eventloop(struct smt_event_loop * loop, int fdsize, int timersize);
/*destroy an event loop*/
int smt_destroy_event_loop(struct smt_event_loop * loop);

#endif
