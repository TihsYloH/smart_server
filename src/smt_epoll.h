#ifndef SMT_EPOLL_H_
#define SMT_EPOLL_H_


#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "smt_ev.h"



#define smt_malloc malloc
#define smt_free     free

typedef int poll_api_time_t;

typedef struct  poll_api_data {
      int epfd;
      struct epoll_event  * events;
} poll_api_data_t;

static int poll_api_create(struct smt_event_loop * loop, int max_size)
{
     struct  poll_api_data  * poll_api;

      poll_api = loop->poll_data = smt_malloc(sizeof(poll_api_data_t));

      if (NULL == poll_api) {
            return -1;
      }

      poll_api->events = smt_malloc(sizeof(struct epoll_event ) * max_size);
      if (poll_api->events == NULL) {
            smt_free(poll_api);
            return -1;
      }

      poll_api->epfd = epoll_create(max_size*2);

      if (poll_api->epfd < 0) {
            smt_free(poll_api->events);
            smt_free(poll_api);
            return -1;
      }
      return 0;
}

static int poll_api_add(struct smt_event_loop * loop,  struct fd_events * events)
{
      struct  poll_api_data  * poll_api;
      int mask;
      struct epoll_event e;

      int op;

      op = events->active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

      mask = events->mask;

      poll_api = loop->poll_data;

      e.events = 0;  //clear


      if (mask & SMT_READABLE)  {
         e.events |= (EPOLLIN);// |  EPOLLERR | EPOLLHUP );

      }
      if (mask & SMT_WRITABLE)  {
         //printf("\nEPOLLOUT\n");
         e.events |= (EPOLLOUT);//| EPOLLERR);
      }
      
      if (mask & SMT_POLLPRI)  {
         e.events |= EPOLLPRI;
      }
      

      e.data.ptr = events;



      if (epoll_ctl(poll_api->epfd,op,events->fd,&e) < 0) {
          perror("\nepoll_ctl\n");
         return -1;
      }

      events->active = 1;

      return 0;
}

static int poll_api_del(struct smt_event_loop * loop,  struct fd_events * events)
{
   struct  poll_api_data  * poll_api;
   struct  epoll_event ee;

   poll_api = loop->poll_data;

   if (!events->active) {
      return -1;
   }
   ee.events = 0;
   if (epoll_ctl(poll_api->epfd,EPOLL_CTL_DEL,events->fd,&ee) < 0)
         return -1;
   events->active = 0;
   return 0;
}

static int poll_api_wait(struct smt_event_loop * loop, poll_api_time_t timeout)
{
      int retval, i;
      int mask;
      struct  poll_api_data  * poll_api;
      struct fd_events * pevents;

      poll_api = loop->poll_data;
    

      retval = epoll_wait(poll_api->epfd, poll_api->events,loop->fd_size,timeout);

      if (retval < 0) {
         if (errno == EINTR) {
               return 0;
         }
         printf("\npoll_api_wait\n");
         return -1;
      }


      for ( i = 0; i < retval; i++) {
         mask = 0;

         pevents = poll_api->events[i].data.ptr;

         if ((poll_api->events[i].events & EPOLLIN) || (poll_api->events[i].events & EPOLLERR)
                 ||(poll_api->events[i].events & EPOLLHUP))   {
  
            mask |= (SMT_READABLE&pevents->mask);
         }
         
         
         if ((poll_api->events[i].events & EPOLLOUT) || (poll_api->events[i].events & EPOLLERR))  {
            
            mask |= (SMT_WRITABLE &pevents->mask);
         }

         /*
         if ((poll_api->events[i].events & EPOLLIN) )   {

            mask |= (SMT_READABLE&pevents->mask);
         }


         if ((poll_api->events[i].events & EPOLLOUT) || (poll_api->events[i].events & EPOLLERR))  {

            mask |= (SMT_WRITABLE &pevents->mask);
         }
         */
         if (poll_api->events[i].events & EPOLLPRI)  {

            mask |= (SMT_POLLPRI&pevents->mask);
         }

          if ( pevents->mask & mask) 
            pevents->proc(loop, pevents->arg,  mask);
      }
      return 0;
}

static int poll_api_destroy(struct smt_event_loop * loop)
{
            struct  poll_api_data  * poll_api;

            poll_api = loop->poll_data;
            close(poll_api->epfd);
            smt_free(poll_api->events);
            smt_free(poll_api);
            return 0;
}

#endif
