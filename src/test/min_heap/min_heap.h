#ifndef MIN_HEAP_H_

#define MIN_HEAP_H_

#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#define MIN_HEAP_DEBUG

#ifdef debug
#undef debug
#endif

#ifdef MIN_HEAP_DEBUG
#define debug(format,args...) printf(format, ##args)
#else
#define debug(format,args...)
#endif
/*



*/
/*             原则 e1 >= e2 返回大于0                                            */
typedef int (*minelm_cmp_pt)(const void * e1, const void * e2);




typedef struct min_heap min_heap_t;





struct min_heap
{
    void **p;
    uint32_t queue_size, size;          //queue_size 队列大小            size元素个数
    minelm_cmp_pt pt;
    int element_size;
};


/*max_elements 最大元素数量   min 最小元素的指针  minelm_cmp_pt pt 元素比较函数 */
void * min_heap_init(int max_elements, void *min, int minsize,
                     minelm_cmp_pt pt);
int  min_heap_destroy(min_heap_t * heap);                                                                       //释放一个最小堆

void *min_heap_resize(min_heap_t * heap, int max);                                                          //大小重新分配
void * min_heap_insert(min_heap_t * heap, void *e);                               //插入一个元素e
void * min_heap_pop(min_heap_t * heap);                                                                         //弹出堆顶元素

void * min_heap_top(min_heap_t * heap);                                                                          //返回堆顶元素

void * min_heap_delete(min_heap_t * heap, void * e);                            //删除一个其中的一个元素e









#endif
