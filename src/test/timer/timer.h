#include "list.h"
#include <stdint.h>
#include <pthread.h>

#define MAX_TIMER_COUNT     100

typedef void* (*RawTimeCallBack)(void *, uint32_t);

typedef int (*timerListFuns) (void *, void *);

enum {
    PERSISTENCE = 0x00,
    ONCE
};

typedef struct raw_timer {
    uint32_t    timeSet;
    uint32_t    timeOut;
    RawTimeCallBack timeCallBack;
    void *arg;
    uint32_t argSize;
    struct list_head entry;

    uint8_t persist;
} Raw_Timer;

typedef struct timer_head {
    struct list_head entry;
    uint32_t nodeCount;
    timerListFuns listEmp;      //��һ������ Timer_Head* �ڶ������� NULL
    timerListFuns addTimer;     //��һ������ Timer_Head* �ڶ������� Raw_Timer *
    timerListFuns optTimer;     //��һ������ Timer_Head* �ڶ������� NULL
    timerListFuns stopTimer;    //��һ������ Timer_Head* �ڶ������� Raw_Timer *
    //timerListFuns updateTimer;
    pthread_mutex_t timerHeadLock;
} Timer_Head;

void *initTimerList();
void destroyTimerList(void *);  // void * ����ͷָ��
/*
typedef struct cmd_timer {
    Raw_Timer timer;

} Cmd_Timer;

typedef struct timer_list {
   Cmd_Timer timer;
   struct timer_list *next;
   uint8_t     timerFlag;
} Timer_List;
*/
