#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>    /*PPSIX 终端控制定义*/
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <asm/types.h>
#include <netinet/ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/uio.h>

#include "modules.h"
#include "serial.h"
#include "tcp_server.h"
#include "net_config.h"
#include "iic.h"
#include "events.h"
#include "zigbee_dev.h"
#include "send.h"
#include "buffer.h"

#define NODEBUG_SER     //是否调试串口

#define DEBUG_LINK      //是否调试链路质量

#ifdef debug_serial
    #undef debug_serial
#endif

#ifdef DEBUG_SER
#define debug_serial(format,args...) do {printf(format, ##args);\
                                    fflush(stdout);}while (0)
#else
#define debug_serial(format,args...)
#endif


#ifdef DEBUG_LINK

#define debug_link(format,args...) do {printf(format, ##args);\
                                    fflush(stdout);}while (0)
#else
#define debug_link(format,args...)
#endif

#define  MAX_SERIAL_RES_BUFFER   300         //串口数据最大限度

#define CON_FILE        "/mnt/con_file"         //配置文件的位置

static uint32_t    live_conn = 0;

static struct list_head free_conn;     //空闲的conn
static struct list_head busy_conn;     //正在使用的conn

static struct list_head trans_list;     //传输队列

/*       发送数据  */
/*数据发送从套接口发送  返回发送的状态 成功返回SEND_OK 无数居或者失败返回SEND_FAILED 发送阻塞返回SEND_BLOCKED*/
static send_state_t socket_packet_snd(void *);
/*数据发送从串口发送  返回发送的状态 成功返回SEND_OK 无数居或者失败返回SEND_FAILED 发送阻塞返回SEND_BLOCKED*/
static send_state_t serial_packet_snd(void *);

/*    发送服务器忙碌  */
static void send_server_busy(int);
static int SockServerInit(uint16_t port);
/*      从套接口读入数据    */
static int readFromSocket(struct connectSock *conn);
static int HandleNewConn(server_t * pserver, int fdListen, int lisState);
/*     发送错误命令帧                 */
static void errSend(uint8_t cmd, struct connectSock * conn);
/*      解析套接口数据             */
static int Dispatch_packet(stuConnSock *conn);
static int disSerialPacket(serDescriptor *ser);



static int serial_init(struct server * pserver, char *dev, int32_t baud);
static int serial_destroy();


/*  从串口中读数据  */
static int readFromSerial(serDescriptor *pserial);

/*   新建一个传输  filename 传输文件的名字  type 0表示读 1表示写                                       */
static int trans_sock_new(smt_ev_loop * loop, conn_sock_t * psock, int fd, int total, int type);
/*          销毁一个传输事件                             */
static void trans_sock_del(trans_sock_t * ptrans);
static void trans_destroy(trans_data_t * p);


static int trans_unit_post(char * filename, conn_sock_t * psock, int type, int total_bytes);

static inline void trans_unit_exp(trans_unit_t * punit);

static trans_unit_t * trans_unit_get(void);
/*                      初始化传输                             */
static int trans_init(trans_data_t * p, smt_ev_loop * ploop);

/*             激活数据传输事件                     */
static inline int trans_active(trans_data_t * p)
{
    //return smt_add_fdevents(&pserver->conn_loop, &pserver->trans_event);
    return 0;
}

static inline int fevent_active(smt_fd_events * pevents)
{
    return (pevents->active == 1);
}

static inline int fevent_mask(smt_fd_events * pevents)
{
    return (pevents->mask);
}

/*            使事件传输事件进入等待状态                              */
static inline int trans_wait(trans_data_t * p)
{
    //return smt_del_fdevents(&pserver->conn_loop, &pserver->trans_event);
    return 0;
}





static uint32_t CalCheck(uint8_t *value, uint32_t size) {
    uint32_t total = 0;
    int i = 0;
    while (i < size)
        total += value[i++];
    return total&0xFF;
}



static inline void free_conn_add(conn_sock_t * psock)
{
    list_add(&psock->entry, &free_conn);
}

static inline void free_conn_del(conn_sock_t * psock)
{
    list_del(&psock->entry);
}

static inline void busy_conn_add(conn_sock_t * psock)
{
    list_add(&psock->entry, &busy_conn);
}
static inline void busy_conn_del(conn_sock_t * psock)
{
    list_del(&psock->entry);
}
static inline int serial_mod_fevent(serial_des_t * pserial, int mask);
/*          添加conn事件  sock*/
static inline int conn_del_fevent(conn_sock_t  * conn);
/*          删除conn事件 */
static inline int conn_add_fevent(conn_sock_t  * conn, int mask);

static inline int conn_del_tevent(conn_sock_t * conn);
static inline int conn_add_tevent(conn_sock_t * conn);

static inline int serial_del_tevent(serial_des_t * pserial);
static inline int serial_add_tevent(serial_des_t * pserial);

static void listener_cb(smt_ev_loop * loop, void * pdata, int mask);//帧听套接子回调函数
static int time_tick_cb(struct smt_event_loop *loop, void *pdata);
static int heart_beat_cb(struct smt_event_loop *loop, void *pdata);
static int server_init(server_t * server, conn_sock_t * psock);

static int server_start(server_t * server);
static int server_stop(server_t * server);
static int server_destroy(server_t * server);

static void serial_cb(smt_ev_loop * loop, void * pdata, int mask);       //串口数据处理
static void conn_cb(smt_ev_loop * loop, void * pdata, int mask);        //连接处理


static void serial_cb(smt_ev_loop * loop, void * pdata, int mask)       //串口数据处理
{
    serial_des_t * pserial = pdata;

    int thisRead;

    debug_serial("\nserial cb\n");
    if (mask & SMT_READABLE) {

        if (pserial->event_cnt < SERIAL_MAX_EVENTS) {

            thisRead = readFromSerial(pserial);
            debug_serial("\nread cb ser->remainPos = %d\n", pserial_des->remainPos);
            if (thisRead < 0) {
                log_error(LOG_ERROR, "------------read from serial err----------");
            }
            debug_serial("\ndisSerialPacket\n");
            disSerialPacket(pserial);
        }
    }

    if (mask & SMT_WRITABLE) {
#ifdef SEND_BY_POLL
       pserial->snd_buf.snd_packet(pserial);
#endif
#ifdef SEND_BY_RCV
       pserial->snd_buf.snd_packet(pserial);
#endif
    }


    return;
}



static void conn_cb(smt_ev_loop * loop, void * pdata, int mask)         //连接处理
{
    conn_sock_t * conn = pdata;
   //

    if (mask & SMT_READABLE) {
        conn->noProbes = 0;

        if (conn->event_cnt < CONN_MAX_EVENTS) {

            if (readFromSocket(conn) < 0) {
                conn_del(conn);                             //不会再有事件了

                //printf("\nconn_cb\n");
                //exit(1);


                return;
            }
            Dispatch_packet(conn);
        }
    }

    if (mask & SMT_WRITABLE) {
#ifdef SEND_BY_POLL
        conn->snd_buf.snd_packet(conn);
#endif
#ifdef SEND_BY_RCV
        if (conn->snd_buf.snd_packet(conn) == SEND_FAILED) {
            conn_del(conn);
            return;
        }
#endif
    }

    if (mask & SMT_POLLPRI) {
        //带外数据
        log_error(LOG_DEBUG, "ou2t of band data");
        conn->noProbes = 0;
    }

    return;
}



static void listener_cb(smt_ev_loop * loop, void * pdata, int mask) //侦听事件
{
    int fd_conn;
    struct server * pserver = pdata;

    if (mask & SMT_READABLE) {
        fd_conn = HandleNewConn(pserver, pserver->listen_fd, pserver->ip_state);
        if (fd_conn <= 0)
            log_error(LOG_NOTICE, "server busy");
    } else {
        log_error(LOG_NOTICE, "listen error");
    }
    return;
}

static int conn_timer_cb(struct smt_event_loop *loop, void *pdata)  //连接命令处理  非重复性事件
{
    conn_sock_t * conn = pdata;
    //printf("\nconn_timer_cb\n");
    //exit(1);

    if (conn->event_cnt < CONN_MAX_EVENTS)
        Dispatch_packet(conn);          //接着处理命令
    else
        conn_add_tevent(conn);
    return 0;
}

static int serial_timer_cb(struct smt_event_loop *loop, void *pdata)  //串口命令处理 非重复性事件
{
    serial_des_t * pserial = pdata;

    if (pserial->event_cnt < SERIAL_MAX_EVENTS)
        disSerialPacket(pserial);
    else
        serial_add_tevent(pserial);
    //    serial_add_tevent(pserial);             //数据没有处理完成 接着处理
    return 0;
}

static int time_tick_cb(struct smt_event_loop *loop, void *pdata) //管理连接定时器 重复事件
{

   // conn_sock_t * psock;
    if (list_empty(&busy_conn))
        return 0;
    return 0;
    //list_for_each_entry(psock, &busy_conn, entry) {
        //timer_admin(psock);                 //更新连接定时器
    //}

    return 0;
}

static int heart_beat_cb(struct smt_event_loop *loop, void *pdata)          //心跳回调
{
    conn_sock_t * psock;
    conn_sock_t * ptmp;
    if (list_empty(&busy_conn))
        return 0;
    list_for_each_entry_safe(psock, ptmp, &busy_conn, entry) {

        psock->noProbes++;
        if (psock->loginLegal < 0)
            psock->noProbes++;
        if (psock->noProbes >= HEART_PROBE_TIMES) {         //心跳到期
            log_error(LOG_NOTICE, "connection time out");
            conn_del(psock);
        }
    }
    return 0;
}

static int server_init(struct server * server, conn_sock_t * psock)
{
   int fdListen, i;
   int flags = 1;

   fdListen = SockServerInit(PORT);
   if (fdListen < 0) {
      log_error(LOG_ERROR, "server init error");
      goto err0;
   }

   if (setsockopt(fdListen, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags)) != 0) {
       log_error(LOG_ERROR, "setsockopt TCP_NODELAY");
   }

   if (setsockopt(fdListen, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags)) < 0) {
       log_error(LOG_ERROR, "setsockopt SO_KEEPALIVE");
   }

   int cnt;
   cnt = 600;           //10分钟
   if (setsockopt(fdListen, SOL_TCP, TCP_KEEPIDLE, (void *)&cnt, sizeof(cnt)) < 0) {
       log_error(LOG_ERROR, "setsockopt TCP_KEEPIDLE");
   }

   cnt = 30;           //每隔30s发送一次探测分节
   if (setsockopt(fdListen, SOL_TCP, TCP_KEEPINTVL, (void *)&cnt, sizeof(cnt)) < 0) {
       log_error(LOG_ERROR, "setsockopt TCP_KEEPINTVL");
   }

   cnt = 5;           //最多发送5次探测分节
   if (setsockopt(fdListen, SOL_TCP, TCP_KEEPCNT, (void *)&cnt, sizeof(cnt)) < 0) {
       log_error(LOG_ERROR, "setsockopt TCP_KEEPCNT");
   }


   server->listen_fd = fdListen;
   INIT_LIST_HEAD(&free_conn);
   INIT_LIST_HEAD(&busy_conn);
   server->ip_state = 0;
   fd_event_init(&server->listen_event);
   timer_event_init(&server->heart_beat_event);
   timer_event_init(&server->tick_gen_event);
   connInit(sttConnSock);
   if (smt_init_eventloop(&server->conn_loop, MAX_LINK_SOCK+5, MAX_LINK_SOCK+10) < 0)        //初始化事件循环
       goto err1;


   fd_event_set(&server->listen_event, SMT_READABLE, fdListen, listener_cb, server);        //加入侦听事件

   if (smt_add_fdevents(&server->conn_loop, &server->listen_event) < 0)
       goto err2;
   timer_event_set(&server->tick_gen_event, CONN_TIMER_TICK, time_tick_cb, NULL, NULL, SMT_TIMEREPEAT); //连接定时器
   if (smt_add_tevents(&server->conn_loop, &server->tick_gen_event) < 0)
       goto err3;
   timer_event_set(&server->heart_beat_event, HEART_BEAT_BEAP, heart_beat_cb, NULL, server, SMT_TIMEREPEAT);    //心跳事件
   if (smt_add_tevents(&server->conn_loop, &server->heart_beat_event) < 0)
       goto err4;

   for (i = 0; i < MAX_LINK_SOCK; i++) {
       list_add(&psock[i].entry, &free_conn);
   }
   if (serial_init(server, SERIAL_DEV, UART_BAUD) < 0)   //初始化串口
       goto err5;

   //if (trans_init(&server->trans_data, &server->conn_loop) < 0)
      // goto err6;

   return 0;

//err6:
  // serial_destroy();
err5:
   smt_del_tevents(&server->conn_loop, &server->heart_beat_event);
err4:
   smt_del_tevents(&server->conn_loop, &server->tick_gen_event);
err3:
   smt_del_fdevents(&server->conn_loop, &server->listen_event);
err2:
   smt_destroy_event_loop(&server->conn_loop);
err1:
   close(fdListen);
   connDestroy(sttConnSock);
err0:
   log_error(LOG_ERROR, "server init error");
   return -1;
}

int get_liveconn(void)
{
    return live_conn;
}

void * tcp_server(void *arg)
{
    server_t * server = malloc(sizeof(server_t));

    if (server == NULL) {
        perror("NO MEM");
        sys_abort();
    }

    if (server_init(server, sttConnSock) < 0) {
        log_error(LOG_ERROR, "server_init error");
        sys_abort();
    }
    server_start(server);
    server_stop(server);
    server_destroy(server);
    free(server);
    return 0;
}

/*
int event_cbs(cmd_event_t * p)
{
    printf("\nevent_cbs\n#######\n*******\n!!!!!!!\n");
    return 0;
}

int event_cbs1(cmd_event_t * p)
{
    char * x = p->buffer[0];
    printf("\nevent_cbs\nxxxxxxxx x = %d\n*******\n!!!!!!!\n", x);
    return 0;
}
*/

int test_pt(sqlite_db_t * sql_db, struct connectSock * psock, void * arg, int size)
{
    char sql[200];

    printf("\n\ntest pt\n\n");

    sql_format(sql, 200, "insert into user values('test user', '123456', 'broofleg@163.com', '15895830938');");


    if (smtdb_opt(sql_db, sql) < 0)
        printf("\ncache_opt_item\n");

    sql_format(sql, 200, "insert into user values('TihsYloH', '123456', 'broofleg@163.com', '15895830938');");
    if (smtdb_opt(sql_db, sql) < 0)
        printf("\ncache_opt_item\n");

    sql_format(sql, 200, "select * from user where user_name = 'TihsYloH';");
    if (smtdb_search(sql_db, sql, NULL, NULL) == 0) {
        printf("\nTihsYloH user find \n");
    } else {
        printf("\nTihsYloH user not find\n");
    }

    sql_format(sql, 200, "select * from user where user_name = 'TihsYloH__';");
    if (smtdb_search(sql_db, sql, NULL, NULL) == 0) {
        printf("\nTihsYloH___ user find \n");
    } else {
        printf("\n\nTihsYloH___ user not find\n");
    }
    return 0;
}

static int server_start(server_t * server)
{
    ipState++;
    server->ip_state = ipState;
    server->server_state = 1;       //开启
    log_error(LOG_NOTICE, "server start");

    if (event_queue_thread(NULL) == NULL) {
        log_error(LOG_EMERG, "event_queue_thread");
        sys_abort();
    }
#ifdef SEND_BY_POLL
    if (WorkThreadCreate(buffer_send_thread, 10, NULL) < 0) {
        log_error(LOG_EMERG, "buffer send thread");
        sys_abort();
    }
#endif

    event_queue_push(EVENT_NORMAL, NULL, test_pt, NULL, 0);
    /*
    char x;
    event_queue_push(EVENT_L0, NULL, event_cbs, NULL, 0);
    x = 1;
    event_queue_push(EVENT_NORMAL, NULL, event_cbs1, &x, 1);
    x = 2;
    event_queue_push(EVENT_NORMAL, NULL, event_cbs1, &x, 1);
    x = 3;
    event_queue_push(EVENT_NORMAL, NULL,   event_cbs1, &x, 1);
    x = 4;
    event_queue_push(EVENT_NORMAL, NULL, event_cbs1, &x, 1);
    x = 5;
    event_queue_push(EVENT_NORMAL, NULL, event_cbs1, &x, 1);
    printf("\nevent_queue_push\n");
    */
    smt_event_loop(&server->conn_loop);
    server->server_state = 0;       //关闭
    return 0;
}
static int server_stop(server_t * server)
{
    struct timespec slptm;

    while (server->server_state != 2) {         //发送线程是否停止
        slptm.tv_sec = 1;
        slptm.tv_nsec = 0;
        nanosleep(&slptm, NULL);
    }


    log_error(LOG_NOTICE, "server end");
    return 0;
}

static int server_destroy(server_t * server)
{
    trans_destroy(&server->trans_data);
    smt_del_tevents(&server->conn_loop, &server->heart_beat_event);
    smt_del_tevents(&server->conn_loop, &server->tick_gen_event);
    smt_del_fdevents(&server->conn_loop, &server->listen_event);
    smt_destroy_event_loop(&server->conn_loop);
    serial_destroy();
    close(server->listen_fd);
    connDestroy(sttConnSock);
    return 0;
}


static inline int serial_add_tevent(serial_des_t * pserial)
{
    timer_event_set(&pserial->tm_event, 100, serial_timer_cb, NULL, pserial, SMT_TIMERONECE);
    if (smt_add_tevents(pserial->loop, &pserial->tm_event) < 0) {
        return -1;
    }
    return 0;
}


static inline int serial_del_tevent(serial_des_t * pserial)
{
    smt_del_tevents(pserial->loop, &pserial->tm_event);
    return 0;
}




static int buffer_send(snd_buffer_t * pbuf, void * buf, int size)
{
    int cur_copy = 0;
    int remain;


    remain = MAX_SND_BUFFER_SIZE - pbuf->cnt;

    if (remain < size) {
        log_error(LOG_NOTICE, "buffer full remain = %d, size = %d", remain, size);

        return -1;
    }
/*
    if (remain == MAX_SND_BUFFER_SIZE) {
        pbuf->head = pbuf->tail = 0;
        memcpy(pbuf->buffer, buf, size);
        pbuf->tail += size;
        pbuf->cnt += size;
    } else if (pbuf->head < pbuf->tail) {

        if (MAX_SND_BUFFER_SIZE - pbuf->tail > size) {
            memcpy(pbuf->buffer+pbuf->tail, buf, size);
            pbuf->tail += size;
            pbuf->cnt += size;
        } else {
            cur_copy += MAX_SND_BUFFER_SIZE - pbuf->tail;
            size -= cur_copy;
            pbuf->cnt += cur_copy;
            memcpy(pbuf->buffer+pbuf->tail, buf, MAX_SND_BUFFER_SIZE - pbuf->tail);
            pbuf->tail = 0;
            memcpy(pbuf->buffer, buf + cur_copy, size);
            pbuf->tail += size;
            pbuf->cnt += size;
        }

    } else {
        memcpy(pbuf->buffer + pbuf->tail, buf , size);
        pbuf->cnt += size;
        pbuf->tail += size;
    }
*/

    if (pbuf->head <= pbuf->tail) {
        if (MAX_SND_BUFFER_SIZE - pbuf->tail > size) {
            memcpy(pbuf->buffer+pbuf->tail, buf, size);
            pbuf->tail += size;
            pbuf->cnt += size;
        } else {
            cur_copy += MAX_SND_BUFFER_SIZE - pbuf->tail;
            size -= cur_copy;
            pbuf->cnt += cur_copy;
            memcpy(pbuf->buffer+pbuf->tail, buf, MAX_SND_BUFFER_SIZE - pbuf->tail);
            pbuf->tail = 0;
            memcpy(pbuf->buffer, buf + cur_copy, size);
            pbuf->tail += size;
            pbuf->cnt += size;
        }
    } else {
       memcpy(pbuf->buffer + pbuf->tail, buf, size);
       pbuf->cnt += size;
       pbuf->tail += size;
    }

    if (pbuf->tail == MAX_SND_BUFFER_SIZE) {
        pbuf->tail = 0;
    }

    return 0;
}

int serial_buffer_send(void * arg, void * buf, int size)    //数据移动到串口发送缓冲区
{
    struct SerialDes * pserial = arg;
    int ret = 0;
    snd_buffer_t * pbuf = &pserial->snd_buf;


    pthread_mutex_lock(&pbuf->lock);
    if (pserial->fd_serial <= 0) {
        pthread_mutex_unlock(&pbuf->lock);
        log_error(LOG_ERROR, "serial no longer exist");
        return -1;
    }
    ret = buffer_send(pbuf, buf, size);
    /*
    if (((pserial->serial_event.mask & SMT_WRITABLE) == 0) && ret == 0) {
        serial_mod_fevent(pserial, SMT_WRITABLE | SMT_READABLE);
    }
    */
#ifdef SEND_BY_POLL
    if (!fevent_active(&pserial->send_event))   //发送成功 如果没有注册则注册   防止频繁注册
        if (send_register(pserial->fd_serial, pserial, serial_cb, &pserial->send_event) < 0)
            log_error(LOG_EMERG, "send_register");
#endif

#ifdef SEND_BY_RCV

    if (!(fevent_mask(&pserial->serial_event) & SMT_WRITABLE))      //没有注册可写事件
        if (serial_mod_fevent(pserial, SMT_READABLE | SMT_WRITABLE)< 0)
            log_error(LOG_ERROR, "serial_mod_fevent\n");
#endif

    pthread_mutex_unlock(&pbuf->lock);

    return ret;
}

sock_error_t sock_buffer_send(void * arg, void * buf, int size)      //数据移动到套接字缓冲区
{
    struct connectSock * sock = arg;
    snd_buffer_t * pbuf = &sock->snd_buf;

    int ret;


    pthread_mutex_lock(&pbuf->lock);
    if (sock->fdSock <= 0) {    //socket 被释放
        pthread_mutex_unlock(&pbuf->lock);
        log_error(LOG_NOTICE, "socket no longer exist");
        return SOCK_FREE;
    }
   // printf("\nsock_buffer_send size = %d\n", size);
    ret = buffer_send(pbuf, buf, size);

#ifdef SEND_BY_POLL
    if (!fevent_active(&sock->send_event)) {        //发送成功 如果没有注册则注册   防止频繁注册
        if (send_register(sock->fdSock, sock, conn_cb, &sock->send_event) < 0) {
            log_error(LOG_ERROR, "send_register");
        }
    }
#else
    #ifdef SEND_BY_RCV
    if (!(fevent_mask(&sock->fd_event) & SMT_WRITABLE)) {           //没有注册可写事件
        if (conn_add_fevent(sock, SMT_WRITABLE /*| SMT_READABLE*/) < 0) {       //写的时候可读  读的时候可写?
            log_error(LOG_ERROR, "conn_add_fevent\n");
        }
    }

    #endif
#endif
    pthread_mutex_unlock(&pbuf->lock);
    return ret;
}


int SerialPacketSend(serial_msg_t *serialHeader,serial_des_t *ser)
{

    uint32_t packLen;

    uint8_t *tagAddr;

    int fd_serial;
    fd_serial = ser->fd_serial;
    if (fd_serial < 1)
        log_error(LOG_NOTICE, "\n-----------serial not open fatal error-----\n");

    tagAddr = (uint8_t *)serialHeader;
    packLen = serialHeader->msg_len;

    if (ser->snd_buf.put_packet(ser, tagAddr, packLen) < 0) {
        log_error(LOG_NOTICE, "send serial err");
        return -1;
    }
    return 0;

}

sock_error_t SockPackSend(tcp_msg_t *msgHeader, int fdConn, struct connectSock *sttParm)
{
    uint32_t packLen;
    uint8_t *tagAddr;
    uint8_t tagBuffer[50];
    sock_error_t sock_error;

    int dataLen;

    if (fdConn <= 0)    //检查socket是否还存在
        return SOCK_FREE;
    if (msgHeader->cmd >= SERVER_ERR) {
        msgHeader->msg_len = htons(8);
        tagAddr = tagBuffer;
        memcpy(tagAddr, msgHeader, TCPHEADER_LEN);
        msgHeader = (tcp_msg_t *)tagAddr;
    }
    else
        tagAddr = (uint8_t *)msgHeader;

    packLen = ntohs(msgHeader->msg_len);//数据包长度
    dataLen = packLen-8;
   // printf("\nsnd\n");
    sock_error = sttParm->snd_buf.put_packet(sttParm, msgHeader, packLen);
   // printf("\nsnd\n");
    return sock_error;

}

int brocast_to_all(tcp_msg_t * pmsg)
{
    conn_sock_t * psock = sttConnSock;
    int i = 0;

    for (i = 0; i < MAX_LINK_SOCK; i++) {
        SockPackSend(pmsg, psock->fdSock, psock);
        psock++;
    }
    return 0;
}

void connInit(stuConnSock *conn)
{

    int i;
    for (i = 0; i < MAX_LINK_SOCK; i++) {
        memset((void*)(conn+i), 0, sizeof(stuConnSock));
        if (pthread_mutex_init(&conn[i].snd_buf.lock, NULL) != 0)
            log_error(LOG_NOTICE, "\npthread_mutex_init err\n");
        (conn+i)->snd_buf.snd_packet = socket_packet_snd;
        (conn+i)->snd_buf.put_packet = sock_buffer_send;

        conn[i].sock_index = i;
        conn[i].addr = i + 1;
        fd_event_init(&(conn+i)->fd_event);         //初始化事件
        fd_event_init(&(conn+i)->send_event);
        timer_event_init(&(conn+i)->tm_event);      //初始化定时器
    }

    return;
}

void connDestroy(stuConnSock * conn)
{
   int i;
   for (i = 0; i < MAX_LINK_SOCK; i++) {
      pthread_mutex_destroy(&conn->snd_buf.lock);
   }
   return;
}
static void SetNonBlock(int fdListen)
{
    int opts;
    if ((opts = fcntl(fdListen, F_GETFL)) < 0)
    {
        perror("fcntl(F_GETFL) error\n");
        exit(1);
    }
    opts |= O_NONBLOCK;
    if (fcntl(fdListen, F_SETFL, opts) < 0)
    {
        perror("fcntl(F_SETFL) error\n");
        exit(1);
    }
}



static void sigAlarm(int signo)
{
    int i;
   //int count;
    //int8_t msg = 10;
    //int errno_back;
    //errno_back = errno;
    uint8_t gheartbeat_fre = 5;
    for (i=0; i< MAX_LINK_SOCK; i++)
    {
        if (sttConnSock[i].fdSock > 0)
        {
            ++sttConnSock[i].noProbes;
        }
    }

    signal(SIGALRM, sigAlarm);
    alarm(gheartbeat_fre);

    //errno = errno_back;
    return;
}


void sig_urg(int signo)
{
    int i;
    int errno_back, count;
    uint8_t msg;
    errno_back = errno;
    for (i = 0; i < MAX_LINK_SOCK; i++) {
        if (sttConnSock[i].fdSock > 0) {
            printf("\n--catch oob \n");

            count = recv(sttConnSock[i].fdSock, &msg, 1, MSG_OOB);

            if (count <= 0) {
                if (errno == EWOULDBLOCK) {

                    sttConnSock[i].noProbes = 0;
                }
            } else if (count > 0)
                sttConnSock[i].noProbes = 0;


        }
    }
    errno = errno_back;
}


void proposeSignal(int gheartbeat_fre)
{
    struct sigaction sa;

    signal(SIGALRM, sigAlarm);
    alarm(gheartbeat_fre);


    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);

/*

    signal(SIGURG, sig_urg);
    sa.sa_flags = 0;
    sigaction(SIGURG, &sa, 0);
*/
}

static int serial_init(server_t * pserver, char *dev, int32_t baud)
{
    int fd_serial;
    fd_serial = init_serial(dev, baud);
    if (fd_serial  < 0) {
        printf("\n----open serial err----\n");
        return -1;
    }

    SetNonBlock(fd_serial);

    memset(&serialDes, 0, sizeof(serDescriptor));
    pthread_mutex_init(&serialDes.snd_buf.lock, NULL);
    serialDes.fd_serial = fd_serial;
    serialDes.snd_buf.snd_packet = serial_packet_snd;
    serialDes.snd_buf.put_packet = serial_buffer_send;
    serialDes.loop = &pserver->conn_loop;

    fd_event_set(&serialDes.serial_event, SMT_READABLE, fd_serial, serial_cb, &serialDes);

    if (smt_add_fdevents(serialDes.loop, &serialDes.serial_event) < 0) {
        log_error(LOG_ERROR, "serial init");
        exit(1);
    }

    timer_event_init(&serialDes.tm_event);

    read(fd_serial, serialDes.recvBuf, MAX_SERIAL_PACKET_LEN);
    //tcflush(fd_serial, TCIOFLUSH);
    pserial_des = &serialDes;
    return fd_serial;
}

static int serial_destroy()
{
   smt_del_fdevents(serialDes.loop, &serialDes.serial_event);
   close(serialDes.fd_serial);
   pthread_mutex_destroy(&serialDes.snd_buf.lock);
   return 0;
}

static int readFromSerial(serDescriptor *ser)
{
    int recvLen = 0;
    int curRead;
    int remain;
    recvLen = ser->remainPos;
    remain = recvLen;
    do
    {
        curRead = read(ser->fd_serial, ser->recvBuf+recvLen, SERIAL_RECV_BUF-recvLen);
        if (curRead < 0)
        {
            if (errno == EINTR)
            {
                printf("\n------errno == EINTR while read fd_serial = %d------\n", ser->fd_serial);
                continue;
            }
            if (errno != EAGAIN)
            {
                debug_serial("\n error ser->remainPos = %d\n", ser->remainPos);
                perror("\nerrno != EAGAIN\n");
                return -1;
            }
            curRead = 0;
        }

#ifdef DEBUG_SERIAL
        int cc;
        for (cc = recvLen; cc < recvLen+curRead; cc++)
            printf(" %x", ser->recvBuf[cc]);
#endif
        debug_serial("\ncur_read = %d\n", curRead);
        recvLen += curRead;
        //PRINTD("\n------curRead = %d------\n", curRead);

    } while (curRead > 0);


    ser->remainPos = recvLen;



    //PRINTD("\n------total recvLen = %d------\n", recvLen);
    return recvLen - remain;       //返回此次读取长度(非当前用户缓冲区数据长度)

}







static int ser_cb(sqlite_db_t * sqlite, conn_sock_t * psock, void * arg, int size)
{
    serial_msg_t * pmsg = arg;
    g_zig_mod[pmsg->module_no].serial_pt[pmsg->cmd](sqlite, psock, pmsg, size);
    return 0;
}

static int disSerialPacket(serial_des_t *ser)
{


    int packLen = 0;
    uint8_t *oriAddr;

    serial_msg_t *pser_msg;
    //int cmd_proposed = 0;
    uint16_t head;


    uint8_t cmd, module_no, opt_code;

    oriAddr = ser->recvBuf;

    debug_serial("pos = %d SERIALHEADER_LEN = %d\n", ser->remainPos, SERIALHEADER_LEN);
    while ( ser->remainPos >= SERIALHEADER_LEN)  {

        if (ser->event_cnt >= SERIAL_MAX_EVENTS) {
            serial_add_tevent(ser);
            break;
        }

        pser_msg = (serial_msg_t *)oriAddr;

        head = ntohs(pser_msg->head);

        debug_serial("\nhead = %x\n", head);

        if (head != 0x2A2F) {
            //debug_serial("\npos = %d\n", ser->remainPos);
            ser->remainPos--;
            oriAddr++;
            continue;
        }



        debug_serial("msg_len = %d\n", pser_msg->msg_len);

        packLen = pser_msg->msg_len;


        debug_serial("serpropose packLen = %d\n", packLen);
        if (packLen <= SERIALHEADER_LEN || packLen > MAX_SERIAL_PACKET_LEN) {
            log_error(LOG_DEBUG, "------PACKLEN NOT MATCH-----");
            ser->remainPos = 0;
            continue;
        }

        if (ser->remainPos <  packLen)
            break;

        if ( cal_check((uint8_t *)pser_msg, packLen - 1) != oriAddr[packLen -1]) {
            log_error(LOG_DEBUG, "---receive serial packet but check err = %d----", oriAddr[packLen -1]);
            oriAddr += packLen;
            ser->remainPos -= packLen;
            continue;
        }


        debug_serial("\n *** pos = %d packLen = %d\n", ser->remainPos, packLen);



        //命令处理
        //printf("\nxxxxx\n");

        //cmd_proposed++;

        cmd = pser_msg->cmd;
        module_no = pser_msg->module_no;
        opt_code = pser_msg->opt_code;

        if (cmd >= UPLOAD) {
            log_error(LOG_DEBUG, "cmd unrecongise");
            oriAddr += packLen;
            ser->remainPos -= packLen;
            continue;
        }

        debug_link("\nLRI = %d,  RSSI = %d\n", pser_msg->resv.info.lri, pser_msg->resv.info.rssi);

        if (module_no < MAX_MODULE_NO) {
            if (event_queue_push(g_zig_mod[module_no].level, NULL, ser_cb, pser_msg, pser_msg->msg_len) < 0) {
                log_error(LOG_NOTICE, "event_queue_push");
                break;
            }
        }

        oriAddr += packLen;
        ser->remainPos -= packLen;

    }
    memmove(ser->recvBuf, oriAddr, ser->remainPos);


    return 0;
}




stuConnSock * conn_del(conn_sock_t * conn)
{




   pthread_mutex_lock(&conn->snd_buf.lock);

   conn_del_fevent(conn);

#ifdef SEND_BY_POLL
   send_unregister(&conn->send_event);
#endif

#ifdef SEND_BY_RCV

#endif
   close(conn->fdSock);
   conn->fdSock = -1;
   conn->state = conn_none;
   conn->snd_buf.head = conn->snd_buf.tail = conn->snd_buf.cnt = 0;
   pthread_mutex_unlock(&conn->snd_buf.lock);

   busy_conn_del(conn);
   free_conn_add(conn);
   live_conn--;             //活动连接计数
   return NULL;
}


#define NDEBUG_BUFFER_SENDT
#ifdef  DEBUG_BUFFER_SENDT
#define debug_buffer_sent(format,args...) do {printf(format, ##args);\
                                            fflush(stdout); } while (0)
#else
#define debug_buffer_sent(format,args...)
#endif

#ifndef USE_IVOEC
static int _buffer_sent(int fd, void * buffer, int size)
{
   int ret;
   int writed = 0;

   do {
       ret = write(fd, buffer, size);
       if (ret < 0) {
          if (EINTR == errno ) {
             continue;
          }
          if (errno == EWOULDBLOCK) {
             return writed;
          }
#ifdef DEBUG_BUFFER_SENDT
          if (errno == EBADF) {
              printf("\nEBADF = %d\n",EBADF );
          }
          perror("\nerror:");
          printf("\nsdddddddd line = %d\n", __LINE__);
          printf("\nerrno = %d\n", errno);
#endif
          return -1;
       }
       writed += ret;
   }
   while (writed < size);
   return ret;
}

static send_state_t buffer_sent(snd_buffer_t * pbuff, int fd)
{
   int total = 0;
   int ret;

   total = pbuff->cnt;


   if (pbuff->head < pbuff->tail) {

         ret = _buffer_sent(fd, pbuff->buffer + pbuff->head, total);

         if (ret >= 0) {
            //printf("\nret = %d\n", ret);
            pbuff->cnt -= ret;
            pbuff->head += ret;


            if (ret < total) {
               return SEND_BLOCKED;
            } else
               return SEND_OK;
         } else {
            debug_buffer_sent("SEND_FAILED line = %d, head = %d, tail = %d cnt = %d",
                              __LINE__, pbuff->head, pbuff->tail, pbuff->cnt);
            pbuff->cnt = pbuff->tail = pbuff->head = 0;

            return SEND_FAILED;
         }

   } else {
      int this_len = MAX_SND_BUFFER_SIZE - pbuff->head;

      ret = _buffer_sent(fd, pbuff->buffer + pbuff->head, this_len);

      if (ret >= 0) {
         pbuff->cnt -= ret;
         pbuff->head += ret;
         if (pbuff->head >= MAX_SND_BUFFER_SIZE) {
            pbuff->head = 0;
         }
         if (ret < this_len) {
            return SEND_BLOCKED;
         }
      } else {
         debug_buffer_sent("SEND_FAILED line = %d, head = %d, tail = %d cnt = %d",
                           __LINE__, pbuff->head, pbuff->tail, pbuff->cnt);
         pbuff->cnt = pbuff->tail = pbuff->head = 0;
         return SEND_FAILED;
      }
      this_len = pbuff->cnt;
      ret = _buffer_sent(fd, pbuff->buffer, this_len);

      if (ret >= 0) {
         pbuff->cnt -= ret;
         pbuff->head += ret;

#ifdef DEBUG_BUFFER_SENDT
         if (pbuff->head >= MAX_SND_BUFFER_SIZE) {
             printf("\nwanginh  g\ndd   MAX_SND_BUFFER_SIZE head = %d\n", pbuff->head);
             exit(1);
         }
#endif


         if (ret < this_len) {
            return SEND_BLOCKED;
         }
      } else {
         debug_buffer_sent("SEND_FAILED line = %d, head = %d, tail = %d cnt = %d",
                           __LINE__, pbuff->head, pbuff->tail, pbuff->cnt);
         pbuff->cnt = pbuff->tail = pbuff->head = 0;
         return SEND_FAILED;
      }
   }

   return SEND_OK;
}
#endif

#ifdef USE_IVOEC


static int _buffer_sent(int fd, struct iovec * piov, int cnt)
{
   int ret;
   int writed = 0;

   do {
       ret = writev(fd, piov, cnt);
       if (ret < 0) {
          if (EINTR == errno ) {
             continue;
          }
          if (errno == EWOULDBLOCK) {
             return writed;
          }
          return -1;
       }
       writed += ret;
   }
   while (0);
   return ret;
}

static send_state_t buffer_sent(snd_buffer_t * pbuff, int fd)
{
    struct iovec iov[2];
    int ret;
    int total = pbuff->cnt;

    if (pbuff->head < pbuff->tail) {
        iov[0].iov_base = pbuff->buffer + pbuff->head;
        iov[0].iov_len = pbuff->cnt;
        ret = _buffer_sent(fd, iov, 1);

        if (ret >= 0) {
           //printf("\nret = %d\n", ret);
           pbuff->cnt -= ret;
           pbuff->head += ret;

           if (ret < total) {
              return SEND_BLOCKED;
           } else {
              pbuff->cnt = pbuff->tail = pbuff->head = 0;
              return SEND_OK;
           }
        } else {
           debug_buffer_sent("SEND_FAILED line = %d, head = %d, tail = %d cnt = %d",
                             __LINE__, pbuff->head, pbuff->tail, pbuff->cnt);
           pbuff->cnt = pbuff->tail = pbuff->head = 0;

           return SEND_FAILED;
        }
    } else {
        iov[0].iov_base = pbuff->buffer + pbuff->head;
        iov[0].iov_len = MAX_SND_BUFFER_SIZE - pbuff->head;

        iov[1].iov_base = pbuff->buffer;
        iov[1].iov_len = pbuff->tail;

        ret = _buffer_sent(fd, iov, 2);

        if (ret == total) {
            pbuff->cnt = pbuff->tail = pbuff->head = 0;
            return SEND_OK;
        }

        if (ret >= 0) {
            if (ret <= iov[0].iov_len) {
                pbuff->head += ret;
            } else {
                ret -= iov[0].iov_len;
                pbuff->head += ret;
            }
            if (pbuff->head == MAX_SND_BUFFER_SIZE)
                pbuff->head = 0;
            return SEND_BLOCKED;

        } else {
            pbuff->cnt = pbuff->tail = pbuff->head = 0;
            return SEND_FAILED;
        }
    }

    return SEND_OK;
}

#endif

/*数据发送从套接口发送  返回发送的状态 成功返回SEND_OK 无数居或者失败返回SEND_FAILED 发送阻塞返回SEND_BLOCKED*/
static send_state_t socket_packet_snd(void * arg)
{
   struct connectSock * psock = arg;
   snd_buffer_t * pbuff = &psock->snd_buf;
   send_state_t state = SEND_FAILED;

   if (pbuff->cnt != 0 && psock->fdSock > 0) {   //no data or no sock
        pthread_mutex_lock(&pbuff->lock);
        state = buffer_sent(pbuff, psock->fdSock);

        if (state != SEND_BLOCKED) {      //没有阻塞表明已经发送完成了
#ifdef SEND_BY_POLL
            send_unregister(&psock->send_event);
#endif
#ifdef SEND_BY_RCV
            if (conn_add_fevent(psock, SMT_READABLE) < 0)
                log_error(LOG_ERROR, "conn_add_fevent");
#endif
        }

        pthread_mutex_unlock(&pbuff->lock);
   }

   return state;
}
/*数据发送从串口发送  返回发送的状态 成功返回SEND_OK 无数居或者失败返回SEND_FAILED 发送阻塞返回SEND_BLOCKED*/
static send_state_t serial_packet_snd(void * arg)
{
   serDescriptor * pserial = arg;
   snd_buffer_t * pbuff = &pserial->snd_buf;
   send_state_t state = SEND_FAILED;

   if (pbuff->cnt != 0) {
        pthread_mutex_lock(&pbuff->lock);
        state = buffer_sent(pbuff, pserial->fd_serial);

        if (state != SEND_BLOCKED) {
#ifdef SEND_BY_POLL
            send_unregister(&pserial->send_event);
#endif
#ifdef SEND_BY_RCV
            if (serial_mod_fevent(pserial, SMT_READABLE) < 0)
                log_error(LOG_ERROR, "serial_mod_fevent");
#endif
        }

        pthread_mutex_unlock(&pbuff->lock);
   }



   return state;
}

/*
static uint8_t *ackRes(conn_sock_t *psock, tcp_msg_t * msg)
{

    uint8_t *ackData = NULL;

    switch (msg->dataHead.optCode) {
    case 0x0:
        break;
    case 0x1:
        break;
    case 0x2:
        break;
    case 0x3:
        break;
    case 0x4:
        break;

    case 0x8000:
        break;
    case 0x8001:
        break;
    case 0x8002:
        break;


    }

    return ackData;
}
*/






/*
 static uint8_t * queryRes(conn_sock_t *conn, tcp_msg_t *msg)
{
    uint16_t optCode;
    uint8_t *ackData = NULL;

    uint8_t buffer[MAX_SERIAL_RES_BUFFER];
    serial_msg_t *ser;

    ser = (serial_msg_t *)(buffer);

    optCode = msg->dataHead.optCode;

    switch (optCode) {
    case 0x0:
        if (event_queue_push(EVENT_NORMAL, conn, sock_query_0000, msg, msg->frameHead.msgLen) < 0) {
            log_error(LOG_NOTICE, "event_queue_push");
        }
        break;
    case 0x1:

        break;
    case 0x2:
        break;
    case 0x3:
        break;
    case 0x4:
        break;

    case 0x8000:   //查询温度

        break;
    case 0x8001:
        break;
    case 0x8002:
        break;

    default:
        break;
    }


    return ackData;
}
*/
/*
#define SET_WITH_ONE_BIT \
                            do {\
                                msgLen = SERIALHEADER_LEN + 1;\
                                formSerialHeaderFromTcp(ser, msg, *netAddr, *endPoint, msgLen ,*extAddr);\
                                ser->dataBuffer[0] = msg->dataBuffer[0];\
                              \
                                SerialPacketSend(ser, &serialDes);\
                                \
                                if (addFreeTimer((Timer_Cmd *)conn, extAddr, &optCode, timerSeriEvent) < 0)\
                                    ERR_OUTPUT("\n--set command 0x0000 addTimer fail---\n");\
                             }  while(0)
*/
#define SET_WITH_ONE_BIT
/*
static uint8_t * setRes(conn_sock_t *conn, tcp_msg_t *msg)
{


    uint8_t *ackData = NULL;

    uint8_t buffer[MAX_SERIAL_RES_BUFFER];
    serial_msg_t * ser;

    ser = (serial_msg_t *)(buffer);

    switch (msg->dataHead.optCode) {
    case 0x0:

        SET_WITH_ONE_BIT;
        break;
    case 0x1:
        SET_WITH_ONE_BIT;
        break;
    case 0x2:

        SET_WITH_ONE_BIT;
        break;
    case 0x3:
        SET_WITH_ONE_BIT;
        break;
    case 0x4:
        break;

    case 0x8000:
        break;
    case 0x8001:
        break;
    case 0x8002:
        break;

    default:
        break;

    }


    return ackData;
}
*/
/*
static uint8_t * uploadRes(conn_sock_t *conn, tcp_msg_t *msg)
{

    //uint16_t optCode;
    uint8_t *ackData = NULL;
    //uint8_t msgLen;
    serial_msg_t *ser;
    uint8_t buffer[MAX_SERIAL_RES_BUFFER];
    ser = (serial_msg_t *)(buffer);

    switch (msg->dataHead.optCode) {
    case 0x0:
        break;
    case 0x1:
        break;
    case 0x2:
        break;
    case 0x3:
        break;
    case 0x4:
        break;

    case 0x8000:
        break;
    case 0x8001:
        break;
    case 0x8002:
        break;
    default:
        break;

    }

    return ackData;
}
*/
static int SockBuffRecv(stuConnSock * conn)
{

    int curRead = 0;
    int this_read = 0;
    int recv_len = conn->remainPos;

    if (MAX_RECV_BUFF == recv_len) {
       log_error(LOG_NOTICE, "\nsocket buffer full\n");
       return 1;
    }
    do
    {
        //printf("\nrecvLen = %d\n", recvLen);
        curRead = recv(conn->fdSock, conn->recvBuf + recv_len, MAX_RECV_BUFF - recv_len >= MAX_RECV_ONCE ?
                          MAX_RECV_ONCE : MAX_RECV_BUFF - recv_len , 0);        //读系统缓冲区 最大到当前用户缓冲区满
        //printf("\nxxxx\n");
        if (curRead < 0)
        {
            if (errno == EINTR)
            {
                printf("\n------errno == EINTR while read fd = %d------\n", conn->fdSock);
                //exit(1);
                continue;
            }
            if (errno != EAGAIN)    //POSIX.1 对于一非阻塞描述符若无数据可读 read返回-1 errno置EAGAIN
            {

                perror("\nerrno = EAGAIN\n");
                //exit(1);
                return -1;
            }
            curRead = 0;
        }
        this_read += curRead;
        //PRINTD("\n------curRead = %d------\n", curRead);

    } while (curRead > 0 && this_read < MAX_RECV_ONCE);




    conn->remainPos += this_read;

    return this_read;       //返回此次读取长度(非当前用户缓冲区数据长度)
}

/*
int deviceMatch(Zigbee_Table * zigbeeTable, TcpData_Head *dataHead, uint16_t *netAddr, uint8_t *endPoint, uint64_t *extAddr)
{

    int i;

    if (0 != dataHead->serial && 0 != dataHead->seq) {
        for (i = 0; i < zigbeeTable->deviceNum; i++) {
            //for (j = 0; j < zigbeeTable->zigDev[i].cmdNum; j++) {
                if (zigbeeTable->zigDev[i].serial == dataHead->serial &&
                        zigbeeTable->zigDev[i].seq == dataHead->seq) {
                    *extAddr = zigbeeTable->zigDev[i].extAddr;
                    *endPoint = zigbeeTable->zigDev[i].endPoint;
                    return (*netAddr = zigbeeTable->zigDev[i].netAddr);
                }
        }

        return -1;
    } else
        *netAddr = 0;
    return 0;
}
*/

static void errSend(uint8_t cmd, struct connectSock * conn)
{
    uint8_t buffer[50];

    tcp_msgform(buffer, cmd, 0, 0, 0, NULL, 0);

    SockPackSend((tcp_msg_t *)buffer, conn->fdSock, conn);
}



static int readFromSocket(struct connectSock *conn)
{
   int thisRead;

   thisRead = SockBuffRecv(conn);

   if (thisRead <= 0)                //判断socket关闭
   {
       //#ifdef DEBUG_DATA
       printf("\n----DispatchPacket--Connection lost: FD = %d this = %d\n", conn->fdSock, thisRead);
       //#endif
       return -1;
   }

   return thisRead;
}



static int net_cb(sqlite_db_t * sqlite, conn_sock_t * psock, void * arg, int size)
{
    tcp_msg_t * pmsg = arg;
    g_zig_mod[pmsg->module_no].net_pt[pmsg->cmd](sqlite, psock, pmsg, size);
    return 0;
}

static int  Dispatch_packet(struct connectSock * conn)
{
    uint8_t * oriAddr;
    tcp_msg_t *msg;
    //serDescriptor *ser;

    uint32_t packLen;


    uint32_t head;
    uint16_t msgLen;


    uint8_t module_no;
    uint8_t opt_code;
    uint8_t cmd;
    /*******************data process ********/
    oriAddr = conn->recvBuf;


    while (conn->remainPos > TCP_PACKET_BASIC_LEN) {

        if (conn->event_cnt >= CONN_MAX_EVENTS) {
            conn_add_tevent(conn);
            break;
        }

        msg = (tcp_msg_t *)oriAddr;

        head = ntohl(msg->head);

        msgLen = ntohs(msg->msg_len);

        if (head != 0x2A2F2A2F) {
            conn->remainPos--;
            oriAddr++;
            continue;
        } else {
            packLen =  msgLen;
            if (packLen > 200) {
                //log_error(LOG_NOTICE, "--obtain a packet larger than 200 = %d---", packLen);
                if (packLen > MAX_PACKET_SIZE)
                    conn->remainPos = 0;
                return 0;
            } else if (packLen < TCPHEADER_LEN) {       //数据包长度小于最小值
                conn->remainPos--;
                oriAddr++;
                continue;
            }
            if (packLen <= conn->remainPos) {

                if (oriAddr[packLen-1] != oriAddr[packLen-1] ) {//计算校验码  //cal_check(oriAddr, packLen-1) != oriAddr[packLen-1]
                    printf("\n---Err check Not Match = %x what recv is %x ---\n", CalCheck(oriAddr, packLen-1), oriAddr[packLen-1]);
                    oriAddr += packLen;
                    conn->remainPos -= packLen;

                    errSend(CHECK_WRONG, conn);

                    continue;
                }


                msg->head = head;
                msg->msg_len = msgLen;
                module_no = msg->module_no;
                opt_code = msg->opt_code;
                cmd = msg->cmd;

                if (conn->loginLegal < 0) {
                /*             用户名和密码验证     非验证密码操作删除          */
                }

                if (cmd >= UPLOAD) {
                    oriAddr += packLen;
                    conn->remainPos -= packLen;
                    log_error(LOG_DEBUG, "cmd unrecongise");
                    continue;
                }

                if (module_no < MAX_MODULE_NO) {
                    if (event_queue_push(g_zig_mod[module_no].level, conn, net_cb, msg, msg->msg_len) < 0) {
                        log_error(LOG_NOTICE, "event_queue_push");
                        break;
                    }
                }

                oriAddr += packLen;
                conn->remainPos -= packLen;
                /***************************************************/


            } else
                break;


        }

    }
    memmove(conn->recvBuf, oriAddr, conn->remainPos);

    return 0;
}



static struct list_head * list_first(struct list_head * list)
{
    struct list_head * l;
    l = list->next;
    return l;
}

static stuConnSock * searchFreeConn(int max)
{
    conn_sock_t * psock = NULL;
    struct list_head * ptmp;
    if (list_empty(&free_conn))
        return NULL;
    else {
        ptmp = list_first(&free_conn);
        psock = container_of(ptmp, conn_sock_t, entry);
    }

    return psock;
}




static void send_server_busy(fd_conn)
{
       int packLen;
       int ret;
       uint8_t tagAddr[50];
       /*
       msg = (tcp_msg_t *)tagAddr;
        msg->frameHead.cmd = SERVER_BUSY;
        msg->frameHead.msgLen = TCPHEADER_LEN;
        msg->dataHead.optCode = 0x0001;
        msg->dataHead.seq = 0x0003;
        msg->dataHead.serial = 0x0002;
        packLen = 8;//数据包长度
        msg->frameHead.head = htonl(0x2A2F2A2F);
        msg->frameHead.msgLen = htons(msg->frameHead.msgLen);
        msg->dataHead.optCode = htons(msg->dataHead.optCode);
        msg->dataHead.serial = htons(msg->dataHead.serial);
        msg->dataHead.seq = htons(msg->dataHead.seq);
        tagAddr[packLen-1] = CalCheck(tagAddr, packLen-1);
        */
        tcp_msgform(tagAddr, SERVER_BUSY, 0, 0, 0, NULL, 0);
        packLen = TCPHEADER_LEN;
        do {
           ret = send(fd_conn, tagAddr, packLen, 0);
           if (ret < 0) {
              if (EINTR == ret) {
                 continue;
              }
              break;
           }
        } while (ret <= 0);
        log_error(LOG_NOTICE, "-------a conn coming but there is no free-------");
}



static inline int snd_buf_init(snd_buffer_t * pbuff, packet_put_pt put_pt, packet_snd_pt  snd_pt)
{
   pbuff->cnt = pbuff->tail = pbuff->head = 0;
   pbuff->snd_packet = snd_pt;
   pbuff->put_packet = put_pt;
   return 0;
}

/*             连接建立                         */
static void conn_ested(conn_sock_t * pconn)
{
    int cnt;
    uint8_t buffer[100];
    uint8_t pdata[20];
    long version = g_dev_version;
    tcp_msg_t * pmsg = (tcp_msg_t *)buffer;
    cnt = zigcon_cnt();

    pdata[3] = (version & 0xFF);

    pdata[2] = ((version>>8) & 0xFF);

    pdata[1] = ((version>>16) & 0xFF);

    pdata[0] = ((version>>24) & 0xFF);

    pdata[4] = (cnt >> 8)&0xFF;
    pdata[5] = cnt & 0xFF;

    tcp_msgform(buffer, 0, 0, 0x11, 0, pdata, 6);
    SockPackSend(pmsg, pconn->fdSock, pconn);

}

conn_sock_t * conn_new(server_t * pserver, int fd_conn, struct sockaddr_in * gclient_addr, int listen_state)
{
   conn_sock_t * conn;

   conn = searchFreeConn(MAX_LINK_SOCK);
   if (!conn) {               //服务器忙碌
        send_server_busy(fd_conn);
        return NULL;
   }

   //memset(conn, 0, sizeof(stuConnSock));
   snd_buf_init(&conn->snd_buf, sock_buffer_send, socket_packet_snd);

   SetNonBlock(fd_conn);
   conn->loginLegal = -1;
   conn->remainPos = 0;
   conn->clientAddr = *gclient_addr;
   conn->noProbes = 0;
   conn->ipState = listen_state;
   conn->loop = &pserver->conn_loop;
   conn->fdSock = fd_conn;
   conn->state = conn_waiting;

   if (conn_add_fevent(conn, SMT_READABLE) < 0) {
       //服务器出错
       perror("conn_add_fevent");
       conn->fdSock = -1;
       log_error(LOG_NOTICE, "smt_add_fdevents");
       return NULL;
   }

   free_conn_del(conn);
   busy_conn_add(conn);
   live_conn++;
   conn_ested(conn);
   return conn;
}


static inline int conn_add_tevent(conn_sock_t * conn)
{
    timer_event_set(&conn->tm_event, 100, conn_timer_cb, NULL, conn, SMT_TIMERONECE);           //延迟100ms处理
    if (smt_add_tevents(conn->loop, &conn->tm_event) < 0) {
        return -1;
    }
    return 0;
}

static inline int conn_del_tevent(conn_sock_t * conn)
{
    smt_del_tevents(conn->loop, &conn->tm_event);
    return 0;
}

static inline int conn_add_fevent(conn_sock_t  * conn, int mask)
{

    fd_event_set(&conn->fd_event, mask | SMT_POLLPRI,  conn->fdSock, conn_cb, conn);//可读事件和带外数据

    if (smt_add_fdevents(conn->loop, &conn->fd_event) < 0) {
        log_error(LOG_EMERG, "smt_add_fdevents");
        return -1;
    }

    return 0;
}

static inline int conn_del_fevent(conn_sock_t  * conn)
{

    smt_del_fdevents(conn->loop, &conn->fd_event);
    return 0;
}

static inline int serial_mod_fevent(serial_des_t * pserial, int mask)
{
    fd_event_set(&pserial->serial_event, mask, pserial->fd_serial, serial_cb, pserial);
    if (smt_add_fdevents(pserial->loop, &pserial->serial_event) < 0) {
        log_error(LOG_EMERG, "smt_add_fdevents");
        return -1;
    }
    return 0;
}


static int HandleNewConn(server_t * pserver ,int fdListen, int lisState)
{
    int fdConn, clientLen;
    struct sockaddr_in gclient_addr;
    conn_sock_t *conn;

    clientLen = sizeof(struct sockaddr_in);
    //memset(&gclient_addr, 0, clientLen);
    do {
       if ((fdConn = accept(fdListen, (struct sockaddr*)&gclient_addr, (socklen_t *)&clientLen)) == -1)
       {
          if (EINTR == errno) {
             log_error(LOG_NOTICE, "accpent interrupted");
             continue;
          }
          if (ECONNABORTED == errno) {
             log_error(LOG_NOTICE, "accpent interrupted");
             fdConn = 0;
             break;
          }
          if (EWOULDBLOCK == errno) {
             log_error(LOG_NOTICE, "connection not ready");
             break;
          }
          log_error(LOG_ERROR, "an unexpected accept error haapen");
          return 0;
       }

       conn = conn_new(pserver, fdConn, &gclient_addr, lisState);
       if (!conn) {
          close(fdConn);

          break;
       }


    } while (0);
    return fdConn;
}



static int SockServerInit(uint16_t port)
{
    int fdListen, val = 1;
    //int fdListen, val = 100;
    struct sockaddr_in gserver_addr;
    if ((fdListen = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error\n");
        return -1;
    }

    bzero(&gserver_addr, sizeof(gserver_addr));
    gserver_addr.sin_family = AF_INET;
    gserver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    gserver_addr.sin_port = htons(port);

    setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
    //setsockopt把socket属性改成SO_REUSEADDR
    //printf("\n-----=*=-----Current HOST IP: %s\n\n", inet_ntoa(gserver_addr.sin_addr));
    setsockopt(fdListen, SOL_SOCKET, SO_RCVLOWAT, &val, sizeof(int));
    setsockopt(fdListen, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(int));
    /*int nSendBuf = 16 * 1024;
    setsockopt(fdListen, SOL_SOCKET, SO_SNDBUF, &nSendBuf, sizeof( int ) );
    int nZero = 0;
    setsockopt(fdListen, SOL_SOCKET, SO_SNDBUF, ( char * )&nZero, sizeof( nZero ) );
    int nNetTimeout = 1000; // 1秒
    setsockopt( socket, SOL_SOCKET, SO_SNDTIMEO, ( char * )&nNetTimeout, sizeof( int ) );*/

    SetNonBlock(fdListen);

    if (bind(fdListen, (struct sockaddr*)(&gserver_addr), sizeof(gserver_addr)) == -1)
    {
        perror("bind error\n");
        return -1;
    }
    if (listen(fdListen, MAX_LINK_SOCK+1) == -1)
    {
        perror("listen error\n");
        return -1;
    }

    return fdListen;
}


int WorkThreadCreate(ptexec threadexec, int prio, void *arg) // 创建线程
{
   pthread_t pid;
   pthread_attr_t attr;
   //int policy;
   int err;
                                                       //线程默认的属性为非绑定、非分离、缺省1M的堆栈、与父进程同样级别的优先级。
   err = pthread_attr_init(&attr);								//线程属性值不能直接设置，须使用相关函数进行操作，
                                                       //初始化的函数为pthread_attr_init，这个函数必须在pthread_create函数之前调用
   if (err != 0)
   {
       perror("\n----WorkThreadCreate--pthread_attr_init err\n");
       return err;
   }

   err = pthread_attr_setschedpolicy(&attr, SCHED_RR);			//SCHED_FIFO --先进先出；SCHED_RR--轮转法；SCHED_OTHER--其他
   if (err != 0)
   {
       perror("\n----pthread_attr_setschedpolicy err\n");
       return err;
   }

   err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);			//设置线程的分离属性
                                                                   //PTHREAD_CREATE_DETACHED -- 分离线程
                                                                   //PTHREAD _CREATE_JOINABLE -- 非分离线程
   if (err == 0)
   {
       err = pthread_create(&pid, &attr, threadexec, arg); //(void*)&sttConnSock[i]);;
       if (err != 0)
       {
           perror("\n----WorkThreadCreate--pthread_create err\n");
           return err;
       }
   }
   err = pthread_attr_destroy(&attr);
   if (err != 0)
   {
       perror("\n----WorkThreadCreate--pthread_attr_destroy err\n");
       return err;
   }
   return 0;
}

#define time_sub(tm1, tm2) ((tm1)->tv_sec > (tm2)->tv_sec ? (tm1)->tv_usec + 1000000 - (tm1)->tv_usec :        \
                                                                ((tm1)->tv_sec == (tm2)->tv_sec ?       \
                                                                         (tm1)->tv_usec - (tm2)->tv_usec: -1))


void send_no_poll(void * arg)          //轮询发送
{
   int blocked;
   send_state_t state;
   struct timespec slptm;
   int cnt;

   do  {
      slptm.tv_sec = 1;
      slptm.tv_nsec = 0;
      nanosleep(&slptm, NULL);
   } while ((ipState == 0));

   while (1) {

      blocked = 0;
      state = serialDes.snd_buf.snd_packet(&serialDes);
      if (SEND_OK != state) {
         blocked++;
      }

      for (cnt = 0; cnt < MAX_LINK_SOCK; cnt++) {//轮询发送网络数据
         state = sttConnSock[cnt].snd_buf.snd_packet(sttConnSock + cnt);
         if (SEND_OK != state) {
            blocked++;
         }
      }

      if (MAX_LINK_SOCK+1 == blocked) {   //全部阻塞或者发送失败
         slptm.tv_sec = 0;
         slptm.tv_nsec = 20000000;      //20ms
         if (nanosleep(&slptm, NULL) == -1) {
            perror("Failed to nanosleep");
            continue;
         }

      }
   }
}


#ifdef SEND_BY_POLL
void send_by_poll(void * arg)
{
    if (send_init() < 0) {
        log_error(LOG_ERROR, "send_init");
        exit(1);
    }

    //send_register(10, NULL, NULL, NULL);

    send_loop();
    send_destroy();
}
#endif
void * buffer_send_thread(void * arg)
{

#ifdef SEND_BY_POLL
   send_by_poll(arg);
#endif

#ifdef SEND_BY_NO_POLL
   send_no_poll(arg);
#endif

#ifdef SEND_BY_RCV
#endif
   return NULL;
}



static int trans_timer_cb(smt_ev_loop * loop, void * arg)
{
    trans_sock_t * ptrans = arg;

    if (!ptrans->transed) {
        trans_sock_del(ptrans);
    } else {
        ptrans->transed = 0;
    }

    return 0;
}

static int trans_data_timer_cb(smt_ev_loop * loop, void * arg)
{
    trans_data_t * pdata = arg;
    trans_unit_t * punit;

    punit = trans_unit_get();
    if (punit != NULL) {
        if (pdata->wait_accept == 2)       {  //正在等待
            trans_unit_exp(punit);
        }
        pdata->wait_accept++;
    } else
        pdata->wait_accept = 0;
    return 0;
}

static int read_from_peer(int fd, uint8_t * pbuf, int max_size, int * recvs)
{
    int cur_read = 0;
    int recv_len = 0;
    do {
        cur_read = recv(fd, pbuf, max_size - recv_len, 0);
        if (cur_read < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        recv_len += cur_read;
    } while (cur_read > 0);

    *recvs = recv_len;

    if (recv_len > 0)
        return 0;
    return -1;
}

static int send_to_peer(int fd, uint8_t * pbuf, int max_size, int * sent)
{
    int cur_write = 0;
    int write_len = 0;

    do {
        cur_write = send(fd, pbuf, max_size - write_len, 0);
        if (cur_write < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                break;
            log_error(LOG_DEBUG, "peer unexpected error");
            return -1;
        }
        write_len += cur_write;
    } while (write_len < max_size);

    *sent = write_len;
    return 0;
}


static void trans_rdwr_cb(smt_ev_loop * loop, void * arg, int mask)
{
#define BUFFER_SIZ  512
    trans_sock_t * ptrans = arg;

    uint8_t buffer[BUFFER_SIZ];



    if (mask & SMT_READABLE) {
        char cmd[100];
        int recv_siz;

        if (read_from_peer(ptrans->fd, buffer, BUFFER_SIZ, &recv_siz) < 0) {
            trans_sock_del(ptrans);                 //传输过程出错 终止传输
        }
        ptrans->transed = 1;                         //收到数据
        ptrans->recv_len += recv_siz;

        if (fwrite(buffer, 1, recv_siz, ptrans->fp) != recv_siz) {
            log_error(LOG_NOTICE, "trans_cb fwrite");
            trans_sock_del(ptrans);                 //写文件出错 终止传输
            return;
        } else if (ptrans->recv_len >= ptrans->total_bytes) {
            if (ptrans->recv_len == ptrans->total_bytes) {
                /*          传输完成                  */
                log_error(LOG_NOTICE, "recv con file successful");
                snprintf(cmd, 100, "mv %s %s", ptrans->filename, CON_FILE);
                system(cmd);
                trans_sock_del(ptrans);
                return;
            } else {
                log_error(LOG_DEBUG, "trans_rdwr_cb");
            }
        }

    } else if (mask & SMT_WRITABLE) {

        int send_siz;
        int sent;

        ptrans->transed = 1;                     //写过数据

        send_siz = fread(buffer, 1, BUFFER_SIZ, ptrans->fp);

        if (send_siz < BUFFER_SIZ) {
            if (ferror(ptrans->fp)) { //出错
                log_error(LOG_DEBUG, "fread ");
            }
/*******************调试***************************************/
             else if (send_siz == 0) {
                log_error(LOG_DEBUG, "send file error");
            }
/**********************************************************/
            clearerr(ptrans->fp);
        }

        if (send_to_peer(ptrans->fd, buffer, send_siz, &sent) < 0) {
            log_error(LOG_NOTICE, "send_to_peer");
            trans_sock_del(ptrans);                 //写套接字出错 终止传输
            return;
        }

        ptrans->send_len += sent;

        if (ptrans->send_len == ptrans->total_bytes) {
            log_error(LOG_NOTICE, "send configuration file successful");
            trans_sock_del(ptrans);
            return;
        }

        if (sent < send_siz) {
            if (fseek(ptrans->fp, ptrans->send_len, 0) < 0) {
                log_error(LOG_NOTICE, "fseek");
                trans_sock_del(ptrans);
                return;
            }
        }

    } else {
        log_error(LOG_DEBUG, "trans_cb");
    }

    return;
}



static uint8_t trans_unit_cnt;

void ______________de_warning()         //没有作用
{
    trans_data_t * p = NULL;
    smt_ev_loop loop;
    loop.fd_cnt = 0;
    trans_unit_post("1", sttConnSock, 1, 1);
    trans_init(p, &loop);
    return;
}

static int trans_unit_post(char * filename, conn_sock_t * psock, int type, int total_bytes)
{
    if (trans_unit_cnt)
        return -1;

    trans_unit_t * punit = malloc(sizeof(trans_unit_t));

    if (punit == NULL) {
        log_error(LOG_DEBUG, "trans_unit_post");
        return -1;
    }
    memcpy(punit->filename, filename, strlen(filename) + 1);
    punit->psock = psock;
    punit->total_bytes = total_bytes;
    punit->type = type;
    list_add_tail(&punit->entry, &trans_list);
    trans_unit_cnt++;
    return 0;
}

static inline void trans_unit_exp(trans_unit_t * punit)
{
    trans_unit_cnt--;
    list_del(&punit->entry);
    free(punit);
}

static trans_unit_t * trans_unit_get(void)
{
    struct list_head * pentry;
    trans_unit_t * punit;

    if (list_empty(&trans_list)) {
        return NULL;
    }
    pentry = trans_list.next;

    punit = list_entry(pentry, trans_unit_t, entry);

    return punit;
}




static void trans_cb(smt_ev_loop * loop, void * arg, int mask)
{
    trans_data_t * pt = arg;
    trans_unit_t * pu;
    int fdConn, clientLen;
    struct sockaddr_in gclient_addr;

    if (mask & SMT_READABLE) {
        pu = trans_unit_get();
        /*
        if (pu == NULL) {
            log_error(LOG_DEBUG, "trans_unit_get");
            trans_destroy(pt);
            if (trans_init(pt, loop) < 0) {
                log_error(LOG_DEBUG, "trans_cb trans_init");
                trans_init(pt, loop);
            }
        } else {
            */
            clientLen = sizeof(struct sockaddr_in);
            do {
               //memset(&gclient_addr, 0, clientLen);
               if ((fdConn = accept(pt->trans_fd, (struct sockaddr*)&gclient_addr,
                                    (socklen_t *)&clientLen)) == -1) {
                  if (EINTR == errno) {
                     log_error(LOG_NOTICE, "accpent interrupted");
                     continue;
                  }
                  if (ECONNABORTED == errno) {
                     log_error(LOG_NOTICE, "accpent interrupted");
                     fdConn = 0;
                     break;
                  }
                  if (EWOULDBLOCK == errno) {
                     log_error(LOG_NOTICE, "connection not ready");
                     break;
                  }
                  log_error(LOG_ERROR, "an unexpected accept error haapen");
                  return;
               }
               if (pu == NULL) {
                   close(fdConn);
                   return;
               }
               if (pu->psock->fdSock > 0) {
                    if (gclient_addr.sin_addr.s_addr == pu->psock->clientAddr.sin_addr.s_addr) {
                            if (trans_sock_new(loop, pu->psock, fdConn, pu->total_bytes, pu->type) < 0) {
                            /*          服务器忙碌               */
                                close(fdConn);
                                log_error(LOG_NOTICE, "trans_sock_new");
                                return;
                            }
                    } else {
                        close(fdConn);
                        log_error(LOG_NOTICE, "client address not match");
                        return;
                    }
               }

               trans_unit_exp(pu);

            } while (0);

        //}



    } else {
        log_error(LOG_DEBUG, "trans_cb");
    }
}

static uint8_t trans_sock_cnt;

static int trans_sock_new(smt_ev_loop * loop,
                          conn_sock_t * psock,
                          int fd,
                          int total_bytes,
                          int type)
{

    char temp[40] = "/tmp/con_file/XXXXXX";         //需要创建/tmp/con_file 文件夹
    int ret;

    if (trans_sock_cnt > 5) {
        log_error(LOG_NOTICE, "too much trans sock");
        return -1;
    }


    trans_sock_t * ptrans = malloc(sizeof(trans_sock_t));
    if (ptrans == NULL) {
        log_error(LOG_DEBUG, "trans_sock_new");
        return -1;
    }

    ptrans->fd = fd;

    ptrans->total_bytes = total_bytes;
    ptrans->recv_len = 0;
    ptrans->send_len = 0;
    ptrans->psock = psock;
    ptrans->ploop = loop;
    ptrans->type = type;
    ptrans->transed = 0;

    fd_event_init(&ptrans->sock_event);

    if (type == 0) {
        memcpy(ptrans->filename, CON_FILE, strlen(CON_FILE) + 1);
        ptrans->fp = fopen(ptrans->filename, "r+");
        if (ptrans->fp == NULL) {
            log_error(LOG_DEBUG, "trans_sock_new");
            free(ptrans);
            return -1;
        }

        fd_event_set(&ptrans->sock_event, SMT_READABLE, fd, trans_rdwr_cb, ptrans);

    } else {
        if ((ret = mkstemp(temp)) < 0) {
            free(ptrans);
            return -1;
        }
        memcpy(ptrans->filename, temp, strlen(temp) + 1);

        ptrans->fp = fdopen(ret, "r+");
        if (ptrans->fp == NULL) {
            log_error(LOG_DEBUG, "trans_sock_new");
            free(ptrans);
            return -1;
        }
        fd_event_set(&ptrans->sock_event, SMT_WRITABLE, fd, trans_rdwr_cb, ptrans);
    }

    if (smt_add_fdevents(loop, &ptrans->sock_event) < 0) {
        fclose(ptrans->fp);
        free(ptrans);
        return -1;
    }

    timer_event_init(&ptrans->timeout_event);
    timer_event_set(&ptrans->timeout_event, 10000, trans_timer_cb, NULL, ptrans, SMT_TIMEREPEAT);   //10s没有数据传输

    if (smt_add_tevents(loop, &ptrans->timeout_event) < 0) {
        smt_del_fdevents(loop, &ptrans->sock_event);
        fclose(ptrans->fp);
        free(ptrans);
        return -1;
    }

    trans_sock_cnt++;

    return 0;
}

static void trans_sock_del(trans_sock_t * ptrans)
{
    trans_sock_cnt--;
    if (ptrans->type == 0)
        unlink(ptrans->filename);
    smt_del_fdevents(ptrans->ploop, &ptrans->sock_event);
    smt_del_tevents(ptrans->ploop, &ptrans->timeout_event);
    fclose(ptrans->fp);
    close(ptrans->fd);
    free(ptrans);
}


static int trans_init(trans_data_t * p, smt_ev_loop * ploop)
{
    int fd_listen;
    fd_listen = SockServerInit(DATA_PORT);



    if (fd_listen < 0) {
        log_error(LOG_DEBUG, "SockServerInit");
        return -1;
    }
    INIT_LIST_HEAD(&trans_list);
    p->trans_fd = fd_listen;
    p->wait_accept = 0;
    p->ploop = ploop;
    fd_event_init(&p->trans_event);
    fd_event_set(&p->trans_event, SMT_READABLE, fd_listen, trans_cb, p);

    if (smt_add_fdevents(ploop, &p->trans_event) < 0) {
        log_error(LOG_DEBUG, "trans_init smt_add_fdevents");
        close(fd_listen);
        return -1;
    }

    timer_event_init(&p->tm_event);
    timer_event_set(&p->tm_event, 5000, trans_data_timer_cb, NULL, p, SMT_TIMEREPEAT);

    if (smt_add_tevents(ploop, &p->tm_event) < 0) {
        log_error(LOG_DEBUG, "smt_add_tevents");
        smt_del_fdevents(ploop, &p->trans_event);
        close(fd_listen);
        return -1;
    }

    return 0;
}

static void trans_destroy(trans_data_t * p)
{
    close(p->trans_fd);
    smt_del_fdevents(p->ploop, &p->trans_event);
    smt_del_tevents(p->ploop, &p->tm_event);
}
