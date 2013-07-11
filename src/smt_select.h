#ifndef SELECT_H_
#define SELECT_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "smt_ev.h"

#define smt_malloc malloc
#define smt_free     free

#define MAX_HANDLE      100             //支持的最大句柄

typedef struct timeval  poll_api_time_t;


typedef struct  poll_api_data {
      fd_set r_set, w_set, e_set;
      fd_set _r_set, _w_set, _e_set;

      void  * dataptr[MAX_HANDLE];
      int max_size;
      int maxfd;


} poll_api_data_t;




static int poll_api_create(struct smt_event_loop * loop, int max_size)
{
     struct  poll_api_data  * poll_api;

     if (max_size > MAX_HANDLE)
         return -1;

      poll_api = loop->poll_data = smt_malloc(sizeof(poll_api_data_t));

      if (NULL == poll_api) {
            return -1;
      }
      memset(poll_api, 0, sizeof(poll_api_data_t));

      FD_ZERO(&poll_api->w_set);
      FD_ZERO(&poll_api->r_set);
      FD_ZERO(&poll_api->e_set);

      FD_ZERO(&poll_api->_e_set);
      FD_ZERO(&poll_api->_r_set);
      FD_ZERO(&poll_api->_w_set);


      poll_api->max_size = MAX_HANDLE > max_size ? max_size : MAX_HANDLE;


      return 0;
}

static int poll_api_add(struct smt_event_loop * loop,  struct fd_events * events)
{
      struct  poll_api_data  * poll_api;
      int mask;
      int fd;


      mask = events->mask;
      fd = events->fd;
      poll_api = loop->poll_data;

      if (fd >= poll_api->max_size)
          return -1;



      if (fd > poll_api->maxfd)
          poll_api->maxfd = fd;

      if (events->active == 1) {
          FD_CLR(events->fd, &poll_api->r_set);
          FD_CLR(events->fd, &poll_api->w_set);
          FD_CLR(events->fd, &poll_api->e_set);
      }

      if (mask & SMT_READABLE)  {
          FD_SET(fd, &poll_api->r_set);

      }

      if (mask & SMT_WRITABLE)  {

         FD_SET(fd, &poll_api->w_set);
      }

      if (mask & SMT_POLLPRI)  {

         FD_SET(fd, &poll_api->e_set);
      }


      poll_api->dataptr[fd] = events;

      events->active = 1;

      return 0;
}

static int poll_api_del(struct smt_event_loop * loop,  struct fd_events * events)
{
   struct  poll_api_data  * poll_api;

   int mask = events->mask;

   poll_api = loop->poll_data;

   if (!events->active) {
      return -1;
   }

   if (mask & SMT_READABLE)  {
      FD_CLR(events->fd, &poll_api->r_set);

   }

   if (mask & SMT_WRITABLE)  {

      FD_CLR(events->fd, &poll_api->w_set);
   }

   if (mask & SMT_POLLPRI)  {

      FD_CLR(events->fd, &poll_api->e_set);
   }

   poll_api->dataptr[events->fd] = NULL;
   events->active = 0;
   return 0;
}


static int poll_api_wait(struct smt_event_loop * loop, poll_api_time_t * tmout)
{
      int retval, i;
      int mask;
      struct  poll_api_data  * poll_api;
      struct fd_events * pevents;

      int max_fd;
      int max_cnt;
      poll_api = loop->poll_data;

      max_fd = poll_api->maxfd;
      max_cnt = max_fd+1;

      poll_api->_e_set = poll_api->e_set;
      poll_api->_w_set = poll_api->w_set;
      poll_api->_r_set = poll_api->r_set;

      retval = select(max_fd+1, &poll_api->_r_set, &poll_api->_w_set, &poll_api->_e_set, tmout);

      if (retval < 0) {
         if (errno == EINTR) {
               return 0;
         }
         perror("\nselect\n");
        // printf("\ntmout.tv_sec = %d, u = %d max_fd = %d\n", tmout->tv_sec, tmout->tv_usec, max_fd);
         return -1;
      }


      for (i = 0; i < max_cnt; i++) {
          mask = 0;
          //printf("\ni = %d ********\n", i);

          pevents = poll_api->dataptr[i];

          if (pevents == NULL)
              continue;
          //printf("\ni = %d~~~~~~~~~~\n", i);
          if (FD_ISSET(i, &poll_api->_r_set)) {
              mask |= (SMT_READABLE&pevents->mask);
          }

          if (FD_ISSET(i, &poll_api->_w_set)) {
              mask |= (SMT_WRITABLE&pevents->mask);
          }

          if (FD_ISSET(i, &poll_api->_e_set)) {
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
            smt_free(poll_api);
            return 0;
}
























#endif
