#ifndef MEMPOOL_H_

#define MEMPOOL_H_



#include <stdint.h>


typedef struct mempool mempool_t;


/*                              */
mempool_t * mempool_init(uint32_t size, float factor, uint32_t count, uint32_t prealloc);

int mempool_destroy(mempool_t *);

void * m_malloc(mempool_t *, uint32_t);

int m_free(void *);

void * m_realloc(mempool_t *, void * ptr, uint32_t);
























#endif
