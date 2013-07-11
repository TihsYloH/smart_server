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
/*             ԭ�� e1 >= e2 ���ش���0                                            */
typedef int (*minelm_cmp_pt)(const void * e1, const void * e2);




typedef struct min_heap min_heap_t;





struct min_heap
{
    void **p;
    uint32_t queue_size, size;          //queue_size ���д�С            sizeԪ�ظ���
    minelm_cmp_pt pt;
    int element_size;
};


/*max_elements ���Ԫ������   min ��СԪ�ص�ָ��  minelm_cmp_pt pt Ԫ�رȽϺ��� */
void * min_heap_init(int max_elements, void *min, int minsize,
                     minelm_cmp_pt pt);
int  min_heap_destroy(min_heap_t * heap);                                                                       //�ͷ�һ����С��

void *min_heap_resize(min_heap_t * heap, int max);                                                          //��С���·���
void * min_heap_insert(min_heap_t * heap, void *e);                               //����һ��Ԫ��e
void * min_heap_pop(min_heap_t * heap);                                                                         //�����Ѷ�Ԫ��

void * min_heap_top(min_heap_t * heap);                                                                          //���ضѶ�Ԫ��

void * min_heap_delete(min_heap_t * heap, void * e);                            //ɾ��һ�����е�һ��Ԫ��e









#endif
