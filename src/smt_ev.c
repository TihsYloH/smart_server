#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <limits.h>

#include "smt_ev.h"


#ifdef HAVE_EPOLL

#include "smt_epoll.h"
#else
#include "smt_select.h"
#endif

typedef struct smt_event_loop smt_event_loop_t;

typedef int (*event_pt_)(struct smt_event_loop * loop, void * events);


#define	tp_cmp0(tvp, uvp) (					\
    (((tvp)->tv_sec == (uvp)->tv_sec) ?				\
     ((tvp)->tv_nsec > (uvp)->tv_nsec) :				\
     ((tvp)->tv_sec > (uvp)->tv_sec))    )


struct timer_events {
    struct timeval expires;
    struct timeval disp;
    smtTimeProc timeProc;
    smtEventFinalizerProc finalizerProc;
    void *arg;
    int mask;
    unsigned char isactive;
};

static int timer_event_pt (const void * tm1, const void * tm2)
{


    const smt_timer_events * ptm1 = tm1;
    const smt_timer_events * ptm2 = tm2;


    if (tp_cmp0(&ptm1->expires, &ptm2->expires)) {
        return 1;
    }
    return 0;
}

int smt_init_eventloop(struct smt_event_loop * loop, int fdsize, int timersize)
{
   smt_timer_events tm_events;
   if (fdsize > SMT_MAX_EVENT_SZIE) {
      fdsize = SMT_MAX_EVENT_SZIE;
   }
   if (timersize > SMT_MAX_TIMER_SZIE) {
      timersize = SMT_MAX_TIMER_SZIE;
   }

   if (fdsize <= 0 || timersize <= 0) {
      return SMT_ERR;
   }

   loop->fd_cnt = 0;
   loop->fd_size = fdsize;
   loop->timer_size = timersize;
   loop->stop = 0;

   tm_events.expires.tv_nsec = LONG_MIN;
   tm_events.expires.tv_sec = LONG_MIN;

   loop->heap = min_heap_init(timersize, &tm_events, sizeof(tm_events), timer_event_pt);
   if (loop->heap == NULL) {
      return SMT_ERR;
   }
   
   if (poll_api_create(loop, fdsize) < 0) {
      min_heap_destroy(loop->heap);
      return SMT_ERR;
   }
    


   return SMT_OK;
   
}

int smt_destroy_event_loop(struct smt_event_loop * loop)
{
   min_heap_destroy(loop->heap);

   poll_api_destroy(loop);

   return SMT_OK;
}

int smt_add_fdevents(struct smt_event_loop * loop, smt_fd_events * events)
{
   if (!((events->mask & SMT_READABLE)) && (!(events->mask & SMT_WRITABLE))) {
      return SMT_ERR;
   }
   if (!events->active) {
      loop->fd_cnt++;
   }

   if (poll_api_add(loop, events) < 0) {
      if (!events->active) {
         loop->fd_cnt--;
      }
      return SMT_ERR;
   }
   
   return SMT_OK;
}

int fd_event_init(struct fd_events * events)
{
   events->active = 0;
   return 0;
}

int fd_event_set(struct fd_events * events, int mask, int fd, smtFileProc  proc, void * arg )
{
   events->fd = fd;
   events->arg = arg;
   events->mask = mask;
   events->proc = proc;

   return 0;
}

int smt_del_fdevents(struct smt_event_loop * loop, struct fd_events * events)
{

   if (poll_api_del(loop, events) < 0)
      return SMT_ERR;
   loop->fd_cnt--;
   return SMT_OK;
}

int timer_event_init(smt_timer_events  * tm)
{

   tm->isactive = 0;
   return SMT_OK;
}

int timer_event_set(smt_timer_events * tm, int ms,
          smtTimeProc timeProc, smtEventFinalizerProc finalizerProc, void * arg, int mask)
{
   long sec;
   if (ms < 0) {
      return SMT_ERR;
   }
   sec = ms / 1000;
   tm->disp.tv_sec = sec;
   tm->disp.tv_nsec = (ms - (sec)*1000)*1000000;
   
   tm->finalizerProc = finalizerProc;
   tm->timeProc = timeProc;
   tm->arg = arg;
   tm->mask = mask;
   return SMT_OK;
}

int smt_add_tevents(struct smt_event_loop * loop, smt_timer_events * tm)
{
   if (tm->isactive) {
      
      return SMT_ERR;
   }

   if (clock_gettime(CLOCK_MONOTONIC, &tm->expires) < 0) {
      return SMT_ERR;
   }
   tm->expires.tv_sec += tm->disp.tv_sec;
   tm->expires.tv_nsec += tm->disp.tv_nsec;

   if (tm->expires.tv_nsec >= 1000000000) {
        tm->expires.tv_nsec -= 1000000000;
        tm->expires.tv_sec += 1;
   }

   if (min_heap_insert(loop->heap, tm) == NULL)
      return SMT_ERR;
   tm->isactive = 1;
   return SMT_OK;
}



int smt_del_tevents(struct smt_event_loop * loop, smt_timer_events * tm)
{
   tm->isactive = 0;
   
   

   if (min_heap_delete(loop->heap, tm) == NULL) {
      return SMT_ERR;
   }

   if (tm->finalizerProc)
      tm->finalizerProc(loop, tm);

   //tm->mask = SMT_TIMERNONE;
   return SMT_OK;
}

int smt_mod_tevents(struct smt_event_loop * loop,  smt_timer_events * tm, int ms, int mask)
{
   if (ms < 0) {
      return SMT_ERR;
   }
   if (tm->isactive) {
      min_heap_delete(loop->heap, tm);
   }
   tm->isactive = 0;

   if (clock_gettime(CLOCK_MONOTONIC, &tm->expires) < 0) {
       return SMT_ERR;
   }



   tm->disp.tv_sec = ms / 1000;
   tm->disp.tv_nsec = (ms - (ms / 1000)*1000)*1000000;

   tm->expires.tv_sec += tm->disp.tv_sec;
   tm->expires.tv_nsec += tm->disp.tv_nsec;

   if (tm->expires.tv_nsec > 1000000000) {
        tm->expires.tv_nsec -= 1000000000;
        tm->expires.tv_sec += 1;
   }

   if (min_heap_insert(loop->heap, tm) == NULL)
      return SMT_ERR;
   tm->isactive = 1;
   return SMT_OK;

}

int smt_loop_stop(struct smt_event_loop * loop)
{
   loop->stop = 1;
   return SMT_OK;
}





#define time_sub(tm1, tm2) ((tm1)->tv_sec > (tm2)->tv_sec ? (tm1)->tv_usec + 1000000 - (tm1)->tv_usec :        \
                                                                ((tm1)->tv_sec == (tm2)->tv_sec ?       \
                                                                         (tm1)->tv_usec - (tm2)->tv_usec: -1))


#define	tp_cmp2(tvp, uvp) (					\
    (((tvp)->tv_sec == (uvp)->tv_sec) ?				\
     ((tvp)->tv_nsec > (uvp)->tv_nsec) :				\
     ((tvp)->tv_sec > (uvp)->tv_sec))    )


int smt_event_loop(struct smt_event_loop * loop)
{
   struct timespec tm;

   smt_timer_events * min, *tm_tmp;
   int min_sleep, i;

   clock_gettime(CLOCK_MONOTONIC, &tm);
   loop->lasttm = tm;


   min_sleep = 1;

   while (!loop->stop) {
     
      //gettimeofday(&tm, NULL);
      clock_gettime(CLOCK_MONOTONIC, &tm);

      if (tp_cmp2((&loop->lasttm), (&tm))) {//time goes back
         for (i = 1; i <= loop->heap->size; i++) {
            tm_tmp = loop->heap->p[i];
            tm_tmp->expires.tv_sec = 0;
            tm_tmp->expires.tv_nsec = 0;
         }
      }
      loop->lasttm = tm;

      do {
         min = min_heap_top(loop->heap);

         if (min != NULL) {
            if (tp_cmp0((&tm), (&min->expires))) {
               min_heap_pop(loop->heap);
               min->isactive = 0;
               if (min->mask & SMT_TIMEREPEAT) 
                  smt_add_tevents(loop, min);

               min->timeProc(loop, min->arg);      
            } else
               break;
         } else
            break;

      } while (1); 

      
      min = min_heap_top(loop->heap);

#ifdef HAVE_EPOLL
      if (min != NULL) {
         min_sleep = (min->expires.tv_sec - tm.tv_sec)*1000 + 
                  (min->expires.tv_nsec - tm.tv_nsec)/1000000;
         if (min_sleep <= 0)
            min_sleep = 0;
      } else {
         
         if (loop->fd_cnt == 0) 
            return 0;
         
         min_sleep = -1;
      }
      
     // min_sleep = 5;//(min_sleep > SMT_MAX_SLEEP ? SMT_MAX_SLEEP : min_sleep);
      if (poll_api_wait(loop, min_sleep) < 0)
         printf("\npoll_api_wait\n");

#else
      struct timeval msleep_;

      if (min == NULL) {
        msleep_.tv_sec = 5;
        msleep_.tv_usec = 500;
      } else {
          msleep_.tv_sec = min->expires.tv_sec - tm.tv_sec;
          msleep_.tv_usec = min->expires.tv_usec - tm.tv_usec;
          if (msleep_.tv_sec > 0) {
              if (msleep_.tv_usec < 0) {
                  msleep_.tv_sec--;
                  msleep_.tv_usec += 1000000;
              }
          } else if (msleep_.tv_sec == 0) {
              if (msleep_.tv_usec < 0) {
                  msleep_.tv_usec = 0;
              }
          } else {
              msleep_.tv_sec = 0;
              msleep_.tv_usec = 0;
          }
      }

      if (poll_api_wait(loop, &msleep_) < 0)
         printf("\npoll_api_wait\n");
#endif
   }
   return 0;
}

