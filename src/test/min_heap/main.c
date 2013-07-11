#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "min_heap.h"
#include <stdint.h>

typedef struct min_elem {
    min_heap_t min;
    struct timeval expires;
    struct timeval disp;
    void *arg;
    int mask;
    unsigned char isactive;
} min_elem_t;


static int timer_event_pt (const void * tm1, const void * tm2)
{
#define	tp_cmp0(tvp, uvp)	(			\
    (((tvp)->tv_sec == (uvp)->tv_sec) ?				\
     ((tvp)->tv_usec > (uvp)->tv_usec) :				\
     ((tvp)->tv_sec > (uvp)->tv_sec))     )

    const min_elem_t * ptm1 = tm1;
    const min_elem_t * ptm2 = tm2;


    if (tp_cmp0(&ptm1->expires, &ptm2->expires)) {
        return 1;
    }
    return 0;
}




int main(int argc, char ** argv)
{
    min_elem_t min;
    min.expires.tv_usec = (-2147483647L - 1);
    min.expires.tv_sec = (-2147483647L - 1);
    min_heap_t * heap;
    heap = min_heap_init(200, &min, sizeof(min), timer_event_pt);

    if (heap == NULL) {
        printf("\nmin_heap_init %d\n", __LINE__);
        exit(1);
    }

    min_elem_t e1, e2, e3, e4, e5, e6, * e;
    e1.expires.tv_usec = 1;
    e1.expires.tv_sec = 2;

    e2.expires.tv_usec = 2;
    e2.expires.tv_sec = 2;

    e3.expires.tv_usec = 3;
    e3.expires.tv_sec = 3;

    e4.expires.tv_usec = 4;
    e4.expires.tv_sec = 2;

    e5.expires.tv_usec = 5;
    e5.expires.tv_sec = 2;

    e6.expires.tv_usec = 0;
    e6.expires.tv_sec = 2;

    min_heap_insert(heap, &e1);
    min_heap_insert(heap, &e2);
    min_heap_insert(heap, &e2);
    min_heap_insert(heap, &e3);
    min_heap_insert(heap, &e4);
    min_heap_insert(heap, &e5);
    min_heap_insert(heap, &e6);


    e = min_heap_top(heap);

    if (e != NULL) {
        printf("\ntop %d %d\n", e->expires.tv_sec, e->expires.tv_usec);
    }

    e = min_heap_top(heap);

    if (e != NULL) {
        printf("\ntop %d %d\n", e->expires.tv_sec, e->expires.tv_usec);
    }

    while ((e = min_heap_pop(heap))) {
        printf("\npop %d %d\n", e->expires.tv_sec, e->expires.tv_usec);
    }

    min_heap_destroy(heap);

    return 0;
}
