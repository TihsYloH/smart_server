#ifndef EVENTS_H
#define EVENTS_H

#include "tcp_server.h"
#include "list.h"
#include "sqlite_db.h"

#define EVENTS_SIZ  512

#define MAX_EBUF_SIZ 768

#define EVENTS_ALL  (EVENTS_SIZ * 2)                //�����������ʱ

#define CONN_MAX_EVENTS     (EVENTS_ALL/(MAX_LINK_SOCK+1))

#define SERIAL_MAX_EVENTS   CONN_MAX_EVENTS

typedef enum {

    EVENT_RLTM = 0,             //������ȼ�
    EVENT_EMSG,
    EVENT_NORMAL,
    EVENT_L0,
    EVENT_L1,
    EVENT_L2,
    EVENT_L3,
    EVENT_L4,
    EVENT_LOW
} event_level_t;

typedef struct cmd_event cmd_event_t;

typedef int (*event_queue_pt)(sqlite_db_t * sql_db, struct connectSock * psock, void * arg, int size);


struct cmd_event {
    struct list_head entry;
    event_level_t level;
    uint16_t weight_value;           //�������ʱ�����ȼ���  level ��weight ��timestamp
    uint32_t timestamp;
    event_queue_pt cb;                  //�ص�����
    struct connectSock * psock;

    uint8_t * buffer;        //
    int buf_siz;
} ;




/*          ʱ��ѹ�������                    */
int event_queue_push(event_level_t          level,
                     struct connectSock *   psock,
                     event_queue_pt         pt,
 //                    uint8_t                cmd,
 //                    uint16_t               opt_code,
                     void *              pdata,
                     int                    siz);

void * event_queue_thread(void * arg);


extern int get_liveconn(void);

#endif // EVENTS_H
