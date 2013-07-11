
typedef void* (*RawTimeCallBack)(void *);

typedef struct raw_timer {
    uint32_t    timeOut;
    uint32_t    timePassed;
    RawTimeCallBack timeCallBack;
    void *arg;
    uint32_t argSize;
} Raw_Timer;

typedef struct cmd_timer {
    Raw_Timer timer;
    /**********************/
} Cmd_Timer;

typedef struct timer_list {
   Cmd_Timer timer;
   struct timer_list *next;
   uint8_t     timerFlag;
} Timer_List;
