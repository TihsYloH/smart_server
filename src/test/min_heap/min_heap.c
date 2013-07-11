#include "min_heap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MIN_DATA        INT32_MIN

#define MIN_HEAP_SIZE   3
#define MAX_HEAP_SIZE   500


#define	tp_cmp1(tvp, uvp)					\
    (((tvp)->tv_sec > (uvp)->tv_sec) ?				\
     1 :				\
     0)


#define	tp_cmp0(tvp, uvp)	(			\
    (((tvp)->tv_sec == (uvp)->tv_sec) ?				\
     ((tvp)->tv_usec > (uvp)->tv_usec) :				\
     ((tvp)->tv_sec > (uvp)->tv_sec))     )

#define	tp_cmp2(tvp, uvp)					\
    ((*tvp > *uvp))

#define elem_cmp(e1, e2) tp_cmp0(e1, e2)











typedef void * (*malloc_mem)(uint32_t);
typedef void (*feee_mem)(void *);
typedef void *( *realloc_mem)(void *, uint32_t);



#define event_realloc realloc;
#define event_malloc  malloc
#define event_free    free

static inline int isFull(const min_heap_t * h)
{
    return h->queue_size == h->size;
}


static inline int isEmpty(const min_heap_t *h)
{
    return h->size == 0;
}


void * min_heap_init(int max_elements, void *min,int min_size,  minelm_cmp_pt pt)
{
    min_heap_t * H;
    if (max_elements < MIN_HEAP_SIZE) {
        debug("\nmin_heap too small\n");
        max_elements = MIN_HEAP_SIZE;
    }

    if (max_elements > MAX_HEAP_SIZE) {
        debug("\nmin_heap too small\n");
        max_elements = MAX_HEAP_SIZE;
    }

    if (min == NULL) {
        debug("minist element not set");
        return min;
    }

    if (pt == NULL) {
        debug("minelm_cmp_pt");
        return pt;
    }



    H = event_malloc(sizeof(min_heap_t));

    if (H == NULL) {
        debug("\nmalloc fail\n");
        return NULL;
    }
    H->pt = pt;
    H->p = event_malloc((max_elements + 1) * sizeof(void *));

    if (H->p == NULL) {
        debug("\nmalloc fail\n");
        event_free(H);
        return NULL;
    }

    H->p[0] = event_malloc(min_size);

    if (H->p[0] == NULL) {
        debug("\nevent_malloc fail\n");
        event_free(H);
        return NULL;
    }

    memcpy(H->p[0], min, min_size);

    H->queue_size = max_elements;
    H->size = 0;
    H->element_size = min_size;

    return H;
}




void * min_heap_pop(min_heap_t * heap)
{
    int i, Child;

    void *min, *last;

    minelm_cmp_pt pt;

    pt = heap->pt;

    if (isEmpty(heap)) {
        debug("\nmin_heap is empty\n");
        return NULL;
    }

    min = heap->p[1];
    last = heap->p[heap->size--];
    for (i = 1; i*2 <= heap->size; i = Child) {
        Child = i*2;
        if (Child != heap->size && pt(heap->p[Child], heap->p[Child + 1]))
        //if (Child != heap->size && heap->p[Child + 1]->expires < heap->p[Child]->expires)
            Child++;
        if (pt(last, heap->p[Child]))
        //if (last->expires > heap->p[Child]->expires)
            heap->p[i] = heap->p[Child];
        else
            break;
    }
    heap->p[i] = last;

    return min;
}


void * min_heap_top(min_heap_t * heap)
{
    if (isEmpty(heap)) {
        debug("\nmin_heap is empty\n");
        return NULL;
    } else {
        return heap->p[1];
    }
}


void * min_heap_delete(min_heap_t * heap, void * el)
{
    int i, Child, pos;
    unsigned char isFind = 0;
    void * last;
    minelm_cmp_pt pt;


    if (isEmpty(heap)) {
        debug("\nmin_heap is empty\n");
        return NULL;
    }

    if (heap->size == 1 && el == heap->p[1]) {
         heap->size--;
         return el;
    }

    pt = heap->pt;

    for (pos = 1; pos <= heap->size; pos++) {

        if (el == heap->p[pos]) {
            isFind = 1;
            break;
        }
    }

    if (0 == isFind)
        return NULL;
    last = heap->p[heap->size--];
    //printf("\npos = %d heap->size = %d\n", pos, heap->size);
    for (i = pos; i*2 <= heap->size; i = Child) {
        Child = i*2;
        if (Child != heap->size && pt(heap->p[Child], heap->p[Child + 1]))
        //if (Child != heap->size && heap->p[Child + 1]->expires < heap->p[Child]->expires)
            Child++;
        if (pt(last, heap->p[Child]))
        //if (last->expires > heap->p[Child]->expires)
            heap->p[i] = heap->p[Child];
        else
            break;
    }
    heap->p[i] = last;
    return el;
}


int  min_heap_destroy(min_heap_t * heap)
{
    void *e;

    while( (e = min_heap_pop(heap)) != NULL) {
        continue;
    }

    event_free(heap->p[0]);

    event_free(heap->p);

    event_free(heap);



    return -1;
}


void *min_heap_resize(min_heap_t * heap, int max)
{
    min_heap_t * tmp;
    if (heap->queue_size >= max)
        return heap;
    else {

        tmp = min_heap_init(max, heap->p[0], heap->element_size, heap->pt);
        if (tmp == NULL)
            return NULL;

        tmp->size = heap->size;
        memcpy(tmp->p, heap->p, (heap->queue_size+1)*(sizeof(void *)));

        event_free(heap->p);
        event_free(heap);

    }
    return tmp;
}


void * min_heap_insert(min_heap_t * heap, void *e)
{
    int i;

    if (isFull(heap)) {
        debug("\n---min_heap full--\n");
        return NULL;
    }

    i = ++heap->size;

    for (;(heap->pt)(heap->p[i/2], e); i /= 2) {
    //for (;heap->p[i/2]->expires > e->expires; i /= 2) {
        heap->p[i] = heap->p[i / 2];

    }



    heap->p[i] = e;


    return e;
}
