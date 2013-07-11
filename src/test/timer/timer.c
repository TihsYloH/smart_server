#include "timer.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define     xBY_MALLOC          //BY_MALLOC 使用动态内存分配


static int timerListEmp(void *, void *);
static int addTimerList(void *, void *);
static int delTimerList(void *, void *);
static int optTimerList(void *, void *);
static int timerListStop(void * , void *);

void *initTimerList()
{
    Timer_Head *listHead = malloc(sizeof(Timer_Head));

    if (listHead == NULL)
        return NULL;

    INIT_LIST_HEAD((&listHead->entry));
    listHead->nodeCount = 0;

    if (pthread_mutex_init(&(listHead->timerHeadLock), NULL) != 0) {
        perror("\n---mutex init err----\n");
        return NULL;
    }
    listHead->listEmp = timerListEmp;
    listHead->addTimer = addTimerList;
    listHead->optTimer = optTimerList;
    listHead->stopTimer = timerListStop;
    return listHead;
}

void destroyTimerList(void *arg)
{
    if (arg == NULL)
        return;
    Timer_Head * head = arg;

    Raw_Timer * t1;
    Raw_Timer * t2;

    list_for_each_entry_safe(t1, t2, (&head->entry), entry)
        delTimerList(head, t1);
    free(head);
}

static int timerListStop(void * arg1, void * arg2)
{
    if (delTimerList(arg1, arg2) == 0)
        return 0;
    return -1;
}


//表空返回0 否则返回-1
static int timerListEmp(void *arg1, void *arg2)
{
    Timer_Head *head = arg1;
    Raw_Timer *t = arg2;
    t = NULL;
    if (list_empty(&head->entry))
        return 0;
    return -1;

}

//成功返回0 失败返回-1  Timer_Head *head Raw_Timer *t
static int addTimerList(void *arg1, void *arg2)
{
    Timer_Head *head = arg1;
    //Raw_Timer *t = arg2;
    if (head->nodeCount >= MAX_TIMER_COUNT)
        return -1;

#ifdef BY_MALLOC
    Raw_Timer *timer = malloc(sizeof(Raw_Timer));
    if (timer == NULL) {
        perror("Raw_Timer *timer = malloc(sizeof(Raw_Timer));");
        return -1;
    }
    memcpy(timer, arg2, sizeof(Raw_Timer));
#else
    Raw_Timer *timer = arg2;
    Raw_Timer *t;
    list_for_each_entry(t, (&head->entry), entry) {
        if (t == timer)
            return -1;
    }

#endif

    struct list_head *newNode = &timer->entry;
    struct list_head *listHead = &head->entry;

    list_add(newNode, listHead);
    head->nodeCount++;
    return 0;
}

//成功返回0 失败返回-1
static int delTimerList(void *arg1, void *arg2)
{
    Timer_Head *head = arg1;
    Raw_Timer *t = arg2;

    list_del(&t->entry);

    if (t != NULL)
#ifdef BY_MALLOC
        free(t);
#else
        ;
#endif
    else
        return -1;
    head->nodeCount--;
    return 0;
}

static int optTimerList(void *arg1, void *arg2)
{
    Timer_Head *head = arg1;
    Raw_Timer * t1 = arg2;
    Raw_Timer * t2;



    list_for_each_entry_safe(t1, t2, (&head->entry), entry) {

        t1->timeOut++;

        if (t1->timeOut >= t1->timeSet) {
            t1->timeCallBack(t1->arg, t1->argSize);
            if (t1->persist == PERSISTENCE) {
                t1->timeOut = 0;
                continue;
            }
            delTimerList(head, t1);
        }
    }
    return 1;
}



