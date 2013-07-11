#include "timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

void *callBack(void *arg , uint32_t size)
{
    if (arg == NULL)
        printf("\n----arg == NULL callBack been called---\n");
    else {
        if (size == sizeof(long))
            printf("\n----curtime = %ld---\n", *(long *)arg);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    long curTime, preTime;

    Timer_Head * head = initTimerList();
    if (head == NULL)
        printf("\ninitTimerList err\n");

    curTime = time(NULL);
    preTime = curTime;

    Raw_Timer t1, t2, t3, t4, t5;
    t1.timeOut = 0;
    t1.timeSet = 2;
    t1.arg = NULL;
    t1.argSize = 0;
    t1.persist = PERSISTENCE;
    t1.timeCallBack = callBack;

    t2.timeOut = 0;
    t2.timeSet = 2;
    t2.arg = &curTime;
    t2.argSize = sizeof(curTime);
    t2.persist = ONCE;
    t2.timeCallBack = callBack;

    t5 = t4 = t3 = t2;
    t5 = t1;
    head->addTimer(head, &t1);
    head->addTimer(head, &t2);
    head->addTimer(head, &t3);
    head->addTimer(head, &t4);
    head->addTimer(head, &t5);

    uint8_t count = 0;
    do {
        count++;
        if (count == 150)
            break;
        if (head->addTimer(head, &t2) >= 0)
            printf("\nlist empty now\n");

        sleep(1);
        curTime = time(NULL);
        if (curTime - preTime > 1) {
            preTime = curTime;
            head->optTimer(head, NULL);
        }
    } while(1);

    destroyTimerList(head);
    return 0;
}
