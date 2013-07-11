#include <stdlib.h>
#include <stdio.h>
#include "mempool.h"
#include "list.h"

#include <pthread.h>
#include <string.h>


#define MAX_MEM_BLOCK   50

#define SLAB_MIN_SIZE   8

#define PREALLOC_SIZE   20

#define MAX_ALLOC_SIZE  30

#define xPTHREAD_SAFE_           //线程安全
#define POOL_DESTROY_          //销毁

#ifdef PTHREAD_SAFE_
#define     MEM_POOL_LOCK(type, ptr, name) (pthread_mutex_lock(&(STATIC_CAST(type, ptr)->name)))
#define     MEM_POOL_UNLOCK(type, ptr, name) (pthread_mutex_unlock(&(STATIC_CAST(type, ptr)->name)))
#else
#define     MEM_POOL_LOCK(type, ptr, name)
#define     MEM_POOL_UNLOCK(type, ptr, name)
#endif




#define     M_CAST(x)    ((Mem_List *)x)
#define     P_CAST(x)    ((Pool_Member *)x)

#define     STATIC_CAST(type, ptr) ((type *) (ptr))

typedef struct mempool_list {
    struct list_head entry;     //空闲表头
#ifdef PTHREAD_SAFE_
    pthread_mutex_t     mem_list_lock;
#endif

    uint16_t used_count;
    uint16_t free_count;

} Mem_List;

typedef struct mempool {
   Mem_List *mem_list;
   float slab_factor;
   uint32_t max_size;
   int32_t *slab_size;
#ifdef PTHREAD_SAFE_
   //pthread_mutex_t     mem_pool_lock;
#endif
} Mem_Pool;

typedef struct member {
    struct list_head entry;
    void * p_list;
    char p_data[0];
} Pool_Member;




static int searchForpos(int32_t *value, int32_t high, int32_t dstValue)
{
    int32_t low = 0;

    high--;
    while (high - low > 1) {

        if (dstValue < value[(high - low)/2]) {
            high = (high+low)/2;
        }
        else if (dstValue > value[(high - low)/2]) {
            low = (high+low)/2;
        }
        else
            return (high + low)/2;

    }
    if (dstValue > value[low])
        return high;
    return low;
}


mempool_t * mempool_init(uint32_t size, float factor, uint32_t count, uint32_t prealloc)
{
    int i,j;

    char * dptr, *ptr;


    if (count == 0)
        return NULL;

    Mem_Pool * pool = malloc(sizeof(Mem_Pool));


    size > SLAB_MIN_SIZE ? size : (size = SLAB_MIN_SIZE, size);

    if (pool == NULL) {
        perror("mempool_init");
        return NULL;
    }

    memset(pool, 0, sizeof(Mem_Pool));

    pool->mem_list = malloc(count * sizeof(Mem_List));
    if (pool->mem_list == NULL) {
        perror("mempool_init");
        free(pool);
        return NULL;
    }

    if (factor <= 1)
        pool->slab_factor = 1.2;
    else
        pool->slab_factor = factor;

    memset(pool->mem_list, 0, sizeof(count * sizeof(Mem_List)));

    pool->max_size = count;

    pool->slab_size = malloc(sizeof(uint32_t)*count);
    if (!pool->slab_size) {
        free(pool);
        return NULL;
    }

    for (i = 0; i < count; i++) {
        INIT_LIST_HEAD(STATIC_CAST(struct list_head , pool->mem_list + i));

        pool->slab_size[i] = size;
        size *= factor;
    }

#ifdef PTHREAD_SAFE_

    for (i = 0; i < count; i++)
        if (pthread_mutex_init(&(pool->mem_list[i].mem_list_lock), NULL) != 0) {
            perror("pthread_mutex_init mempool");
            return NULL;
        }
#endif

    if (prealloc) {

        for (i = 0; i < count; i++) {
            dptr = malloc((size + sizeof(Pool_Member)) * PREALLOC_SIZE);
            if (dptr == NULL) {
                perror("\npoll member alloc fail\n");
                return NULL;
            }

            for (j = 0; j < PREALLOC_SIZE; j++ ) {
                ptr = dptr + (size + sizeof(Pool_Member))*j;
                STATIC_CAST(Pool_Member, ptr)->p_list = &pool->mem_list[i];
                list_add((struct list_head *)ptr, &(pool->mem_list[i].entry));
                pool->mem_list[i].free_count++;
            }
            size *= factor;
        }
    }


    return pool;
}




int mempool_destroy(Mem_Pool * mem_pool)
{
    int i;

    if (!mem_pool)
        return -1;

    for (i = 0; i < mem_pool->max_size; i++) {
            if (mem_pool->mem_list[i].used_count != 0)
                return -1;
    }
    if (!mem_pool->slab_size)
        free(mem_pool->slab_size);
    if (!mem_pool->mem_list)
        free(mem_pool->mem_list);

    free(mem_pool);
    mem_pool = NULL;

    return 0;
}

void * m_malloc(Mem_Pool * mem_pool, uint32_t size)
{
    int count;
    void * dptr;

    count = searchForpos(mem_pool->slab_size, mem_pool->max_size, size);//log((size*1.0)/SLAB_MIN_SIZE)/log(mem_pool->slab_factor);           ///////////

    if (count >= mem_pool->max_size) {
        printf("\nm_alloc fail too large\n");
        return NULL;
    }



    MEM_POOL_LOCK(Mem_List, (mem_pool->mem_list+count), mem_list_lock);

    if ((mem_pool->mem_list)[count].free_count == 0) {
        if ((mem_pool->mem_list)[count].free_count + (mem_pool->mem_list)[count].used_count < MAX_ALLOC_SIZE) {
            dptr = malloc(size + sizeof(Pool_Member));
            if (dptr == NULL)
                perror("\nmalloc fail\n");
            else {
                (mem_pool->mem_list)[count].used_count++;
                MEM_POOL_UNLOCK(Mem_List, (mem_pool->mem_list+count), mem_list_lock);
                P_CAST(dptr)->p_list = mem_pool->mem_list+count;
                dptr = (char * )dptr + sizeof(Pool_Member);
                return dptr;
            }
        }
        MEM_POOL_UNLOCK(Mem_List, (mem_pool->mem_list+count), mem_list_lock);
        return NULL;
    }

    dptr = container_of((mem_pool->mem_list)[count].entry.next, Pool_Member, entry);

    dptr = ((Pool_Member*)dptr)->p_data;

    list_del((mem_pool->mem_list)[count].entry.next);

    (mem_pool->mem_list)[count].free_count--;
    (mem_pool->mem_list)[count].used_count++;

    MEM_POOL_UNLOCK(Mem_List, (mem_pool->mem_list+count), mem_list_lock);



    return dptr;

}

#define     GETP_DISP(x, y)  ((M_CAST((x)) - M_CAST((y))))

void * m_realloc(Mem_Pool * mem_pool, void * ptr, uint32_t size)
{

    void * dptr;
    uint32_t slab_size;

    ptr = (char *)ptr - sizeof(Pool_Member);


    slab_size = mem_pool->slab_size[GETP_DISP(P_CAST(ptr)->p_list, mem_pool->mem_list)];

    MEM_POOL_LOCK(Mem_List, STATIC_CAST(Pool_Member, ptr)->p_list, mem_list_lock);


    if ( slab_size >= size) {

        MEM_POOL_UNLOCK(Mem_List, STATIC_CAST(Pool_Member, ptr)->p_list, mem_list_lock);

        return ptr + sizeof(Pool_Member);

    } else {
        if ((dptr = m_malloc(mem_pool, size)) != NULL)                        //线程锁???
            memcpy(dptr, P_CAST(ptr)->p_data, slab_size - sizeof(Pool_Member));
        MEM_POOL_UNLOCK(Mem_List, STATIC_CAST(Pool_Member, ptr)->p_list, mem_list_lock);
        m_free(ptr + sizeof(Pool_Member));
        return dptr;
    }

    MEM_POOL_UNLOCK(Mem_List, STATIC_CAST(Pool_Member, ptr)->p_list, mem_list_lock);

}


int m_free(void *dptr)
{
    Pool_Member * mem;


    mem = container_of(dptr, Pool_Member, p_data);

    MEM_POOL_LOCK(Mem_List, mem->p_list, mem_list_lock);

    list_add(&mem->entry, &(((Mem_List *)mem->p_list)->entry));

    ((Mem_List *)mem->p_list)->free_count++;
    ((Mem_List *)mem->p_list)->used_count--;

    MEM_POOL_UNLOCK(Mem_List, mem->p_list, mem_list_lock);

    return 0;
}
