
#ifndef TCP_SERVER_H_
#define TCP_SERVER_H_
#include <time.h>
#include <stdint.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <asm/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "smt_ev.h"

#include "log.h"
#include "list.h"
#include "protocol.h"

#define     NOSEND_BY_POLL

#define     SEND_BY_RCV

#define     USE_IVOEC           //�ۼ�Щ�򻯷���


#define     MAX_RECV_ONCE       512

#define     DATA_PORT           7001        //�������Ӷ˿�

#define     PORT                7000        //�������Ӷ˿�

#define     MAX_LINK_SOCK       100
#define     MAX_RECV_BUFF       1024
#define     MAX_SEND_BUFF       2048

#define     MAX_PROCESS_ONCE    3
#define     MAX_SIM_QUEUE_SIZE  20
#define     MAX_PACKET_SIZE     600




#define     MAX_SERIAL_CMD_LEN  300
#define     SERIAL_SEND_BUF     2000
#define     SERIAL_RECV_BUF     2000

#define     MAX_CMD_SIZE        5

#define     TIME_TICK_INTERVAL    200000  //��λ us



#define     MAX_SERIAL_PACKET_LEN   100         //��������

#define     TCP_PACKET_BASIC_LEN    7       //TCP ����С����

#define     MAX_PROPOSE_CMD_ONCE    5      //һ����ദ���������

#define     MAX_RECENT_GUESTS        100        //������������α�����

#define     TIMER_BUFFER_SIZE       300         //��ʱ����������С

#define     MAX_SND_BUFFER_SIZE     25600    //���ͻ�������С




#define     CLOSE_SOCKET_DELAY   20   //25 * 10 = 2.5s * 5 = 12.5s

#define     SOCKET_IDLE_DELAY      10   //10s �޻�ر�socket
#define     HEART_BEAT_BEAP         224000   //ÿ��4s���һ��
#define     HEART_PROBE_TIMES       5       //��������  5*4 = 20s
#define     CONN_TIMER_TICK        2000    //ÿ�������Ӹ���һ�����Ӷ�ʱ��
#define     NORMAL_TIME_OUT        3   //3*2000 = 6000ms 3 ��


#define     MAX_IR_BUF             1024      //������󳤶�

#define     MAX_IR_SEQ             12

#define serial_des_t serDescriptor          //����
#define timer_list_t struct timerList       //���Ӷ�ʱ���б�
#define timer_cmd_t  Timer_Cmd              //��ʱ������
#define conn_sock_t  struct connectSock     //����
#define server_t     struct server




typedef enum {
   SEND_OK,
   SEND_BLOCKED,
   SEND_FAILED
} send_state_t;

typedef  unsigned long sys_time_tick_t;

typedef int (*packet_put_pt)(void *, void *, int);
typedef send_state_t (*packet_snd_pt)(void *);       //�������ݻص�
typedef struct timerList timer_List;
typedef struct connectSock stuConnSock;

typedef int (*queueFunp)(void *, void *);
typedef void* ptexec(void *arg);
typedef void * (*timerPro)(void *);
typedef int (*ipMaskFun)(void *, void *, void *);

/*****************buffer send state*************************/



/*************cmd type*********************/
enum {
    ACK=0x00,
    QUERY,
    SET,
    UPLOAD
};

/*************conn state*********************/
enum {
    conn_none,
    conn_waiting,
    conn_working,
    conn_processing,
    conn_ending
};

/****************server error*****************/

enum {
    SERVER_ERR = 0xE1,
    SERVER_BUSY = 0xE2,
    DATA_WRONG = 0xE3,
    CHECK_WRONG = 0xE4
};

/******************timer busy****************************/
enum {
    TIMER_IDLE = 0x00,
    TIMER_BUSY = 0x01
};

/*     socket send error state                 */

typedef enum {
    SOCK_NONE = 0,
    SOCK_FREE = -2,
    SOCK_FULL = -1
} sock_error_t;

/********************************************************/

/***************ip mask**********************/

typedef struct {
    struct {
        uint32_t    ip;                 //��ֹ��ip
        time_t      lastVisitTime;      //�ϴη���ʱ��
        uint32_t    codeErrTimes;       //��ʱ����������������
        time_t      startForbidTime;    //��ֹ��ʼʱ��
        uint32_t    forbidLast;         //��Ҫ��ֹ��½��ʱ��
    } ipInfo[MAX_RECENT_GUESTS];
    ipMaskFun   processIpIn;            //����������֤���ip (void *, void *, void *);  //Ip_Mask
    pthread_mutex_t ipTableLock;        //���α���
} Ip_Mask;

/*************************************/







typedef struct snd_buffer{
    pthread_mutex_t lock;
    packet_snd_pt snd_packet;           //�������Ƶ��������ص�����
    packet_put_pt put_packet;           //�����ݴӻ��������ͳ�ȥ�Ļص�����
    uint8_t buffer[MAX_SND_BUFFER_SIZE];
    int head;
    int tail;
    int cnt;
} snd_buffer_t;



struct connectSock
{

    int32_t fdSock;                                   //socket
    int32_t loginLegal;                               //login Legal
    struct sockaddr_in clientAddr;                   //
    uint32_t remainPos;                      //
    uint8_t recvBuf[MAX_RECV_BUFF];                    //
    snd_buffer_t snd_buf;        //���ͻ�����
    struct list_head  entry;                 //����λ��
    smt_fd_events fd_event;    //�������
    smt_fd_events send_event;            //����   �������������̵߳�ʱ��ʹ��
    smt_timer_events tm_event;  //��������
    smt_timer_events conn_tick;     //���Ӵ��ʱ��
    smt_ev_loop * loop;
    int32_t noProbes;                                 //心跳
    int32_t ipState;

    struct ir_buffer {
        int     ir_cnt;
        uint8_t ir_buffer[MAX_IR_BUF];
        uint8_t serial;             //���ڵȴ���ϵ�к�
        uint8_t seq_received[MAX_IR_SEQ+1];       //�Ѿ����յ������
        uint8_t last_seq;
        uint8_t key_type;               //����1
        uint8_t key_word;               //����2
        uint16_t key_code;              //�������
    } ir_buffer;

    uint16_t event_cnt;         //�¼�����
    uint8_t sock_index;                                  //��ַ    sock ����������
    uint8_t state;
    uint8_t addr;               //�ն��ڷ������ϵĵ�ַ
};

typedef struct module_queue_s {

} module_queue_t;

typedef struct SerialDes {
    int32_t fd_serial;
    uint16_t remainPos;
    uint8_t recvBuf[SERIAL_RECV_BUF];
    uint8_t state;
    snd_buffer_t snd_buf;

    smt_fd_events serial_event;
    smt_fd_events send_event;           //�������������̵߳�ʱ��ʹ��
    smt_timer_events tm_event;

    smt_ev_loop * loop;
    int cwd;                            //���Է��͵���������
    uint16_t event_cnt;
} serDescriptor;


typedef struct trans_unit_s {
    struct list_head entry;
    char filename[40];
    conn_sock_t * psock;

    int total_bytes;
    int type;
}trans_unit_t;

typedef struct trans_sock_s {
    int fd;
    char filename[40];
    int type;
    int recv_len;
    int send_len;
    int total_bytes;
    FILE * fp;
    conn_sock_t * psock;
    smt_ev_loop * ploop;
    smt_fd_events    sock_event;
    smt_timer_events timeout_event;     //��ʱ
    int transed;
} trans_sock_t;



/*
typedef struct trans_data_s {
    smt_ev_loop trans_loop;

    smt_timer_events tm_event;
    uint8_t trans_state;
} trans_data_t;
*/
typedef struct trans_data_s {
    int trans_fd;
    int wait_accept;
    smt_fd_events trans_event;              //���ݴ����¼�
    smt_timer_events tm_event;              //��ʱ�¼�
    smt_ev_loop * ploop;
} trans_data_t;






struct server {
    smt_ev_loop conn_loop;
    smt_fd_events listen_event;             //��������֡���¼�



    trans_data_t trans_data;

    smt_timer_events heart_beat_event;        //��ⶨʱ��
    smt_timer_events tick_gen_event;          //������ʱ��ʱ��
    int listen_fd;

    int ip_state;
    uint8_t server_state;
};

struct connectSock sttConnSock[MAX_LINK_SOCK];

serDescriptor serialDes;

serial_des_t * pserial_des;







           //����ѭ��

int ipState;        //ip״̬ ��������״̬
uint32_t sysTick;        //���������������
long g_dev_version;
//sys_time_tick_t sys_time_tick;  //ʱ������


/************�ɹ�����0 data Ϊ Queue_Data ������***********************/



/******************************************/

extern int WorkThreadCreate(ptexec threadexec, int prio, void *arg);

/*    ���ݷ����߳�          */
extern void * buffer_send_thread(void * arg);

extern void connInit(stuConnSock *conn);

extern void connDestroy(stuConnSock * conn);

/*      �½�һ���׽��Ӳ��ҽ������֡��        */
extern stuConnSock * conn_new(server_t * pserver, int fd, struct sockaddr_in *cli_addr, int listen_state);
/*      ɾ��һ���׽ӿڲ������Ƴ�              */
extern stuConnSock * conn_del(stuConnSock * conn);




//extern void * process_conn(void *arg);
/*         �Ӷ�Ӧ�׽ӿڷ�������             */
extern int SockPackSend(tcp_msg_t * msg, int fdConn, conn_sock_t *sttParm);


/*         �Ӵ��ڷ�������                   */
extern int SerialPacketSend(serial_msg_t * msg,serial_des_t *ser);

/*
int deviceMatch(Zigbee_Table * zigbeeTable, TcpData_Head *dataHead, uint16_t *netAddr,
                                uint8_t *endPoint, uint64_t *extAddr);
*/
/*     ���Ӵ��ڴ�������      */
void * tcp_server(void *arg);
/*    ���ݷ���     ����ֵSOCK_FREE  socket���ͷŴ���   ����ֵSOCK_FULL   ��������  */
sock_error_t sock_buffer_send(void * arg, void * buf, int size);

/*      �������ݷ���*/
int serial_buffer_send(void * arg, void * buf, int size);

/*        ȡ�û���ӵ�����          */
int get_liveconn(void);

extern int brocast_to_all(tcp_msg_t * msg);
extern void sys_abort();





static inline int buffer_empty(snd_buffer_t * pbuf)
{
    return pbuf->cnt == 0;
}

#endif
