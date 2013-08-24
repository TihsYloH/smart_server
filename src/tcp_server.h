
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

#define     USE_IVOEC           //聚集些简化发送


#define     MAX_RECV_ONCE       512

#define     DATA_PORT           7001        //数据连接端口

#define     PORT                7000        //控制连接端口

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

#define     TIME_TICK_INTERVAL    200000  //单位 us



#define     MAX_SERIAL_PACKET_LEN   100         //串口最大包

#define     TCP_PACKET_BASIC_LEN    7       //TCP 包最小长度

#define     MAX_PROPOSE_CMD_ONCE    5      //一次最多处理的命令数

#define     MAX_RECENT_GUESTS        100        //保留的最大屏蔽表数量

#define     TIMER_BUFFER_SIZE       300         //定时器缓冲区大小

#define     MAX_SND_BUFFER_SIZE     25600    //发送缓冲区大小




#define     CLOSE_SOCKET_DELAY   20   //25 * 10 = 2.5s * 5 = 12.5s

#define     SOCKET_IDLE_DELAY      10   //10s 无活动关闭socket
#define     HEART_BEAT_BEAP         224000   //每隔4s检测一次
#define     HEART_PROBE_TIMES       5       //心跳次数  5*4 = 20s
#define     CONN_TIMER_TICK        2000    //每隔两秒钟更新一次连接定时器
#define     NORMAL_TIME_OUT        3   //3*2000 = 6000ms 3 次


#define     MAX_IR_BUF             1024      //红外最大长度

#define     MAX_IR_SEQ             12

#define serial_des_t serDescriptor          //串口
#define timer_list_t struct timerList       //连接定时器列表
#define timer_cmd_t  Timer_Cmd              //定时器命令
#define conn_sock_t  struct connectSock     //连接
#define server_t     struct server




typedef enum {
   SEND_OK,
   SEND_BLOCKED,
   SEND_FAILED
} send_state_t;

typedef  unsigned long sys_time_tick_t;

typedef int (*packet_put_pt)(void *, void *, int);
typedef send_state_t (*packet_snd_pt)(void *);       //发送数据回调
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
        uint32_t    ip;                 //禁止的ip
        time_t      lastVisitTime;      //上次访问时间
        uint32_t    codeErrTimes;       //短时间密码输入错误次数
        time_t      startForbidTime;    //禁止开始时间
        uint32_t    forbidLast;         //需要禁止登陆的时间
    } ipInfo[MAX_RECENT_GUESTS];
    ipMaskFun   processIpIn;            //处理密码验证后的ip (void *, void *, void *);  //Ip_Mask
    pthread_mutex_t ipTableLock;        //屏蔽表锁
} Ip_Mask;

/*************************************/







typedef struct snd_buffer{
    pthread_mutex_t lock;
    packet_snd_pt snd_packet;           //把数据移到缓冲区回调函数
    packet_put_pt put_packet;           //把数据从缓冲区发送出去的回调函数
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
    snd_buffer_t snd_buf;        //发送缓冲区
    struct list_head  entry;                 //链表位置
    smt_fd_events fd_event;    //处理接收
    smt_fd_events send_event;            //发送   仅当开启发送线程的时候使用
    smt_timer_events tm_event;  //处理命令
    smt_timer_events conn_tick;     //连接存活时间
    smt_ev_loop * loop;
    int32_t noProbes;                                 //蹇冭烦
    int32_t ipState;

    struct ir_buffer {
        int     ir_cnt;
        uint8_t ir_buffer[MAX_IR_BUF];
        uint8_t serial;             //正在等待的系列号
        uint8_t seq_received[MAX_IR_SEQ+1];       //已经接收到的序号
        uint8_t last_seq;
        uint8_t key_type;               //类型1
        uint8_t key_word;               //类型2
        uint16_t key_code;              //红外编码
    } ir_buffer;

    uint16_t event_cnt;         //事件计数
    uint8_t sock_index;                                  //地址    sock 的数组索引
    uint8_t state;
    uint8_t addr;               //终端在服务器上的地址
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
    smt_fd_events send_event;           //仅当开启发送线程的时候使用
    smt_timer_events tm_event;

    smt_ev_loop * loop;
    int cwd;                            //可以发送的命令数量
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
    smt_timer_events timeout_event;     //超时
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
    smt_fd_events trans_event;              //数据传输事件
    smt_timer_events tm_event;              //超时事件
    smt_ev_loop * ploop;
} trans_data_t;






struct server {
    smt_ev_loop conn_loop;
    smt_fd_events listen_event;             //控制连接帧听事件



    trans_data_t trans_data;

    smt_timer_events heart_beat_event;        //检测定时器
    smt_timer_events tick_gen_event;          //产生定时器时钟
    int listen_fd;

    int ip_state;
    uint8_t server_state;
};

struct connectSock sttConnSock[MAX_LINK_SOCK];

serDescriptor serialDes;

serial_des_t * pserial_des;







           //连接循环

int ipState;        //ip状态 网络侦听状态
uint32_t sysTick;        //启动以来的秒计数
long g_dev_version;
//sys_time_tick_t sys_time_tick;  //时钟心跳


/************成功返回0 data 为 Queue_Data 型数据***********************/



/******************************************/

extern int WorkThreadCreate(ptexec threadexec, int prio, void *arg);

/*    数据发送线程          */
extern void * buffer_send_thread(void * arg);

extern void connInit(stuConnSock *conn);

extern void connDestroy(stuConnSock * conn);

/*      新建一个套接子并且将其加入帧听        */
extern stuConnSock * conn_new(server_t * pserver, int fd, struct sockaddr_in *cli_addr, int listen_state);
/*      删除一个套接口并将其移除              */
extern stuConnSock * conn_del(stuConnSock * conn);




//extern void * process_conn(void *arg);
/*         从对应套接口发送数据             */
extern int SockPackSend(tcp_msg_t * msg, int fdConn, conn_sock_t *sttParm);


/*         从串口发送数据                   */
extern int SerialPacketSend(serial_msg_t * msg,serial_des_t *ser);

/*
int deviceMatch(Zigbee_Table * zigbeeTable, TcpData_Head *dataHead, uint16_t *netAddr,
                                uint8_t *endPoint, uint64_t *extAddr);
*/
/*     连接串口处理例程      */
void * tcp_server(void *arg);
/*    数据发送     返回值SOCK_FREE  socket被释放错误   返回值SOCK_FULL   缓冲区满  */
sock_error_t sock_buffer_send(void * arg, void * buf, int size);

/*      串口数据发送*/
int serial_buffer_send(void * arg, void * buf, int size);

/*        取得活动连接的数量          */
int get_liveconn(void);

extern int brocast_to_all(tcp_msg_t * msg);
extern void sys_abort();





static inline int buffer_empty(snd_buffer_t * pbuf)
{
    return pbuf->cnt == 0;
}

#endif
