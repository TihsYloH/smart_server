#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "log.h"
#include "events.h"
#include "list.h"
#include "min_heap.h"
#include "sqlite_db.h"


#define xDEBUG_EVENT

#ifdef DEBUG_EVENT
#define debug_event(format,args...) do {printf(format, ##args);\
                                fflush(stdout); } while (0)
#else
#define debug_event(format,args...)
#endif


#define EVENT_LOCK(h) pthread_mutex_lock(&(h)->lock);
#define EVENT_UNLOCK(h) pthread_mutex_unlock(&(h)->lock);


#define QUEUE_THREADS   2

enum {
    EVENT_ALIVE = 0,
    EVENT_SLEEP
};

typedef struct event_queue {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    min_heap_t * heap;

    smpslab_t * event_slab;
    volatile int event_state;           //确保线程间的同步
    int queue_no;

    sqlite_db_t * sql_db;
} event_queue_t;






static  uint32_t timestamp = 0;
static  uint32_t weightness = 0;

static event_queue_t pq[QUEUE_THREADS];



typedef void *(*thread_pt)(void *arg);
extern int WorkThreadCreate(thread_pt threadexec, int prio, void *arg);










/*                   删除事件               */
static inline void cmd_event_del(event_queue_t * pq, cmd_event_t * pe);

/*           等待事件到来               */
static int event_queue_wait(event_queue_t *);

/*              处理线程弹出事件                                */
static inline cmd_event_t * event_queue_pop(event_queue_t *);


/*      初始化队列                */
static int event_queue_init(void);

static int event_queue_destroy(event_queue_t *);






int cmd_event_init(cmd_event_t * p, struct list_head * head)
{
    memset(p, 0, sizeof(cmd_event_t));

    list_add(&p->entry, head);

    return 0;
}

static inline void * list_first(struct list_head * head)
{
    if (list_empty(head))
        return NULL;
    return head->next;
}

static inline cmd_event_t * cmd_event_new(event_queue_t * pq, int size)
{
    cmd_event_t * pe;

    pe = smpslab_malloc(pq->event_slab);

    if (size != 0) {
        pe->buffer = malloc(size);
        if (pe->buffer == NULL) {
            smpslab_free(pq->event_slab, pe);
            pe = NULL;
        }
    } else
        pe->buffer = NULL;

    return pe;
}

static inline void cmd_event_del(event_queue_t * pq, cmd_event_t * pe)
{
    if (pe->psock != NULL)
        pe->psock->event_cnt--;
    else
        pserial_des->event_cnt--;

    if (pe->buffer != NULL)
        free(pe->buffer);

    smpslab_free(pq->event_slab, pe);


}


static int event_cmp(const void * e1, const void * e2)
{
    const cmd_event_t * pe1 = e1;
    const cmd_event_t * pe2 = e2;


    //printf("\npe1->level = %d, pe2->level = %d\n", pe1->level, pe2->level);
    if (pe1->level < pe2->level)
        return 1;
    else if (pe1->level == pe2->level) {
        if (pe1->weight_value < pe1->weight_value)
            return 1;
        else if (pe1->weight_value == pe2->weight_value) {
            if (pe1->timestamp > pe2->timestamp)
                return 1;
        }
    }


    return 0;
}



static void inline event_queue_wakeup(event_queue_t * peq)
{
    if (peq->event_state == EVENT_SLEEP)
        pthread_cond_broadcast(&peq->cond);
    return;
}

static inline int get_tindex(uint32_t weight)
{
#if (((QUEUE_THREADS - 1) & QUEUE_THREADS) == 0)
    return (weight & (QUEUE_THREADS - 1));             //工作线程数量为2的指数时候
#else
    return weight % QUEUE_THREADS;
#endif
}

int event_queue_push(event_level_t          level,
                     struct connectSock *   psock,
                     event_queue_pt         pt,
 //                    uint8_t                cmd,
 //                    uint16_t               opt_code,
                     void *              pdata,
                     int                    siz)
{
    int ret = 0;
    //int max_conn = 0;
    uint32_t weight = 0;
    struct cmd_event * pe;
    event_queue_t * peq;
    uint16_t * p_value;
    timestamp++;
    //printf("\ntimestamp = %u\n", timestamp);
    if (siz > MAX_EBUF_SIZ) {
        log_error(LOG_NOTICE, "MAX_EBUF_SIZ");
        return -1;
    }

    weight = ++weightness;

    if (psock != NULL) {
        weight = psock->addr;
        p_value = &psock->event_cnt;
    } else {
        p_value = &pserial_des->event_cnt;
    }

    peq = pq + get_tindex(weight);          //平衡相应的队列


    EVENT_LOCK(peq);
    debug_event("\nstart push ok line = %d\n", __LINE__);
    do {
        pe = cmd_event_new(peq, siz);


        if (pe == NULL) {
            if (psock == NULL) {
                peq = pq + get_tindex(weight + 1);
                pe = cmd_event_new(peq, siz);
            }
            if (pe == NULL) {
                ret = -1;
                log_error(LOG_NOTICE, "cmd queue full queue.no = %d", peq->queue_no);
                break;
            }
        }



        pe->level = level;
 //      pe->cmd = cmd;
 //       pe->opt_code = opt_code;
        pe->weight_value = ++(*p_value);
        pe->timestamp = timestamp;
        pe->cb = pt;
        pe->psock = psock;
        memcpy(pe->buffer, pdata, siz);

        //printf("\npe->level = %d, pe->timestamp = %u\n",level, timestamp);
        if (min_heap_insert(peq->heap, pe) == NULL) {
            log_error(LOG_ERROR, "min_heap_inisert !!!!");
            cmd_event_del(peq, pe);
        }


    } while (0);
//     printf("\nccc\n");
    event_queue_wakeup(peq);
    debug_event("\nend push ok line = %d\n", __LINE__);
    EVENT_UNLOCK(peq);

    return ret;
}

static inline cmd_event_t * event_queue_pop(event_queue_t * pq)
{
    cmd_event_t * pe;

    EVENT_LOCK(pq);
    pe = min_heap_pop(pq->heap);
    EVENT_UNLOCK(pq);
    return pe;
}


static void maketimeout(struct timespec *tsp, long seconds)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    tsp->tv_sec = now.tv_sec;
    tsp->tv_nsec = now.tv_usec * 1000;

    tsp->tv_sec += seconds;
}

static int event_queue_wait(event_queue_t * peq)
{
    int ret = 0;
    cmd_event_t * pe;
    struct timespec tsp;
    long seconds = 50;
    EVENT_LOCK(peq);
    do {
     //   printf("\n1\n");
        if ((pe = min_heap_top(peq->heap)) != NULL)
            break;
     //   printf("\n2\n");
        peq->event_state = EVENT_SLEEP;
    //    printf("\n3\n");
     //   pthread_cond_wait(&peq->cond, &peq->lock);
        maketimeout(&tsp, seconds);
        pthread_cond_timedwait(&peq->cond, &peq->lock, &tsp);
     //   printf("\n4\n");
        if ((pe = min_heap_top(peq->heap)) == NULL) {
            log_error(LOG_DEBUG, "pthread_cond_wait");
            ret = -1;
        }
      //  printf("\n5\n");
        peq->event_state = EVENT_ALIVE;
    } while (0);
    EVENT_UNLOCK(peq);
    return ret;
}




int event_queue_init(void)
{
    cmd_event_t event;

    event_queue_t * peq = pq;

    int i;

    event.level = -100000;
    event.timestamp = 0;

    for (i = 0; i < QUEUE_THREADS; i++) {
        peq = pq + i;
        memset(peq, 0, sizeof(event_queue_t));




        peq->event_slab = smpslab_init(EVENTS_SIZ, sizeof(struct cmd_event));

        if (peq->event_slab == NULL) {
            log_error(LOG_DEBUG, "smpslab_init");
            return -1;
        }

        peq->heap = min_heap_init(EVENTS_SIZ, &event, sizeof(event), event_cmp);
        if (peq->heap == NULL) {
            free(peq->event_slab);
            log_error(LOG_EMERG, "min_heap_init");
            return -1;
        }
        peq->queue_no = i;
        pthread_mutex_init(&peq->lock, NULL);
        pthread_cond_init(&peq->cond, NULL);

    }

    return 0;
}

int event_queue_destroy(event_queue_t * peq)
{
    int i;

    for (i = 0; i < 1; i++) {
        peq += i;
        pthread_mutex_destroy(&peq->lock);
        pthread_cond_destroy(&peq->cond);
        min_heap_destroy(peq->heap);
        smpslab_destroy(peq->event_slab);
        //sqlite_db_close(peq->sql_db);
    }
    return 0;
}


#define MAX_DEL_EVENTS_CNTS  30

static inline int del_events(int cnt, cmd_event_t * events[], event_queue_t * peq)
{
    int c = 0;
    if (cnt >= MAX_DEL_EVENTS_CNTS) {

        EVENT_LOCK(peq);
        while(c < cnt) {
            cmd_event_del(peq, events[c]);
            c++;
        }
        EVENT_UNLOCK(peq);
    }
    return c;
}

static inline void del_all_events(int cnt, cmd_event_t * events[], event_queue_t * peq)
{
    EVENT_LOCK(peq);
    while ((cnt)--) {
        cmd_event_del(peq, events[cnt]);
    }
    EVENT_UNLOCK(peq);
}

/*      工作者线程        */
static void * event_worker(void * arg)
{
    event_queue_t * peq = arg;
    struct cmd_event * pe;
    struct cmd_event * events[EVENTS_SIZ];
    int cnt;

    peq->sql_db = malloc(sizeof(*(peq->sql_db)));       //在当前线程中打开数据库连接
    if (peq->sql_db == NULL) {
        log_error(LOG_ERROR, "NO MEM");
        exit(1);
    }

    if (sqlite_db_open(peq->sql_db, SQL_DB) < 0) {      //在当前线程中打开数据库连接
        log_error(LOG_ERROR, "event_queue_init");       //！！！出错
    }

    //sleep(1);
    do {
       // printf("\nevent_worker\n");

        cnt = 0;

        while ((pe = event_queue_pop(peq)) != NULL) {
            pe->cb(peq->sql_db, pe->psock, pe->buffer, pe->buf_siz);
            events[cnt++] = pe;
            cnt -= del_events(cnt, events, peq);
        }
        del_all_events(cnt, events, peq);

        event_queue_wait(peq);

        //sleep(1);

    } while (1);

    sqlite_db_close(peq->sql_db);
    free(peq->sql_db);

    event_queue_destroy(peq);
    return NULL;
}

void * event_queue_thread(void * arg)
{
    int i;

    event_queue_t * peq = pq;

    if (event_queue_init() < 0) {
        log_error(LOG_ERROR, "event_queue_init");
        return NULL;
    }
    for (i = 0; i < QUEUE_THREADS; i++) {
        if (WorkThreadCreate(event_worker, 10, peq+i) < 0)
            return NULL;
    }

    return peq;
}
