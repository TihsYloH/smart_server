#include <stdlib.h>
#include  <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "modules.h"
#include "tcp_server.h"
#include "protocol.h"
#include "log.h"
#include "buffer.h"
#include "zigbee_dev.h"
#include "events.h"

#define  SERIAL_BUFFER  300
#define  SERIAL_WARN    250

#define  DEBUG_MODULES

#ifdef debug
    #undef debug
#endif

#ifdef DEBUG_MODULES
#define debug(format,args...) do {printf(format, ##args);\
                                fflush(stdout); } while (0)
#else
#define debug(format,args...)
#endif


#define     EXT_ADDRLEN   8


static uint8_t  ir_serial;          //红外的系列号

extern int brocast_to_all(tcp_msg_t * pmsg);


static serial_msg_t * tcp_serial(void * buffer, tcp_msg_t * tcp_msg, int addr, void * data, int len);

static void * net_mod_ack(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{

    return NULL;
}





static serial_msg_t * tcp_to_serial(void * buffer, tcp_msg_t * tcp_msg, int addr)
{

    serial_msg_t * s;
    zig_def_t    pdef;
    int ret;
    uint16_t net_addr;
    int size = tcp_msg->msg_len - TCPHEADER_LEN;
    uint8_t * pdata = tcp_msg->pdata;


    ret = zigsrch_ext(tcp_msg->ext_addr, &pdef);



    if (ret < 0) {
        log_error(LOG_NOTICE, "no such device");
        net_addr = 0;

        if (net_addr == 0) {      //终端向未配置的节点发送
            zig_un_t * pun = zigsrch_un(tcp_msg->ext_addr);
            if (pun != NULL) {
                net_addr = pun->net_addr;
                printf("\nnet_addr = %X\n", net_addr);
            }
           // net_addr = 0;
        }


    } else {
        net_addr = pdef.net_addr;
        debug("\ndst net_addr = %d ext_addr = %llX tcp_msg->ext_addr = %llx\n",
              net_addr, pdef.ext_addr, tcp_msg->ext_addr);
    }



    s = serial_msgform(buffer, tcp_msg->cmd, tcp_msg->module_no,
                       tcp_msg->opt_code, tcp_msg->ext_addr, addr,net_addr,
                       pdata, size);

#ifdef DEBUG_MODULES
    printf("\n");
    int i;

    for (i = 0; i < s->msg_len; i++) {
        printf("%x  ", ((uint8_t *)s)[i]);
    }

    printf("\n");
#endif
    return s;
}

static tcp_msg_t * serial_to_tcp(void * buffer, serial_msg_t * pmsg)
{
    tcp_msg_t * t;
    uint16_t net_addr;
    zig_def_t  pdef;
    int ret;
    uint64_t ext_addr = 0;

    int size = pmsg->msg_len - SERIALHEADER_LEN;
    uint8_t * pdata = pmsg->pdata;

    net_addr = pmsg->net_addr;

   // printf("\nxxxxxxxxxxx\n");


    ret = zigsrch_net(net_addr, &pdef);
    if (ret < 0) {
        zig_un_t * pun = zigsrch_unnet(net_addr);
        if (pun != NULL) {//未配置的节点
            ext_addr = pun->ext_addr;
        } else {            //第一次上电的节点
            ext_addr = pmsg->resv.ext_addr;
            size -= EXT_ADDRLEN;
            pdata += EXT_ADDRLEN;
            log_error(LOG_NOTICE, "\nnet addr not exist size = %d\n", size);
            if (size < 0)
                size = 0;
        }
    } else {
        debug("\nnet_addr = %x pdef->net_addr = %d\n", net_addr, pdef.net_addr);
        ext_addr = pdef.ext_addr;
    }

    t = tcp_msgform(buffer, pmsg->cmd, pmsg->module_no, pmsg->opt_code, ext_addr, pdata, size);
    return t;
}

void * net_mod_query(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{

    uint8_t buffer[SERIAL_BUFFER];
    tcp_msg_t * tcp_msg = arg;
    serial_msg_t * serial_msg;
    conn_sock_t * psock = conn;

    if (size > SERIAL_WARN) {
        log_error(LOG_NOTICE, "\nmessage too long\n");
        return NULL;
    }

    debug("\nnet_mod_query module = %d, opt_code = %d, line:%d\n", tcp_msg->module_no, tcp_msg->opt_code, __LINE__);

    serial_msg = tcp_to_serial(buffer, tcp_msg, psock->addr);



    SerialPacketSend(serial_msg, pserial_des);

    return NULL;
}



static void * net_mod_set(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    uint8_t buffer[SERIAL_BUFFER];
    tcp_msg_t * tcp_msg = arg;
    serial_msg_t * serial_msg;
    conn_sock_t * psock = conn;

    /*
    if (size > SERIAL_WARN) {
        log_error(LOG_NOTICE, "\nmessage too long\n");
        return NULL;
    }
    */

    debug("net_mod_set ext_addr = %llX, module = %d, opt_code = %d, \
          addr = %d line:%d",
          tcp_msg->ext_addr,tcp_msg->module_no, tcp_msg->opt_code, psock->addr,  __LINE__);
    serial_msg = tcp_to_serial(buffer, tcp_msg, psock->addr);
    SerialPacketSend(serial_msg, pserial_des);
    return NULL;
}


/*         通用函数第一个参数为NULL                 */
static void * serial_mod_ack(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    uint8_t buffer[SERIAL_BUFFER];
    conn_sock_t * psock;
    tcp_msg_t * tcp_msg;
    serial_msg_t * serial_msg = arg;
    int innet_addr;

    /*
    if (size > SERIAL_WARN) {
        log_error(LOG_NOTICE, "\nmessage too long\n");
        return NULL;
    }
    */

    innet_addr = serial_msg->addr;
    if (innet_addr > MAX_LINK_SOCK) {
        log_error(LOG_DEBUG, "innet_addr");
        return NULL;
    }

    debug("serial_mod_ack module = %d, opt_code = %d, line:%d", serial_msg->module_no, serial_msg->opt_code, __LINE__);

    tcp_msg = serial_to_tcp(buffer, serial_msg);

    if (innet_addr == 0) {      //广播
        brocast_to_all(tcp_msg);
        return NULL;
    }

    psock = &sttConnSock[innet_addr-1];
    SockPackSend(tcp_msg, psock->fdSock, psock);

    return NULL;
}


static void * serial_mod_query(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
/*
    uint8_t buffer[SERIAL_BUFFER];

    tcp_msg_t * tcp_msg = arg;
    serial_msg_t * serial_msg;

    serial_msg = serial_msgform(buffer, tcp_msg->module_no, tcp_msg->opt_code, tcp_msg->ext_addr,
                                NULL, 0);
    SerialPacketSend(serial_msg, pserial_des);
*/
    return NULL;
}

static void * serial_mod_set(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
/*
    uint8_t buffer[SERIAL_BUFFER];
    tcp_msg_t * tcp_msg = arg;
    serial_msg_t * serial_msg;

    serial_msg = serial_msgform(buffer, tcp_msg->module_no, tcp_msg->opt_code, tcp_msg->ext_addr,
                                NULL, 0);
    SerialPacketSend(serial_msg, pserial_des);
*/
    return NULL;

}


void * serial_local_ack(sqlite_db_t * sqlite, void * c, void * arg, int size)
{


    uint8_t buffer[SERIAL_BUFFER];
    conn_sock_t * psock;
    tcp_msg_t * tcp_msg;
    serial_msg_t * serial_msg = arg;
    zig_def_t pdef;
    int innet_addr;
    int i = 0;
    uint64_t ext_addr;
    uint16_t net_addr;
    uint8_t module_no;
    int ret;

    innet_addr = serial_msg->addr;
    if (innet_addr > MAX_LINK_SOCK) {
        log_error(LOG_DEBUG, "innet_addr");
        return 0;
    }

#ifdef DEBUG_MODULES
    ext_addr = getaddr_byte(serial_msg->pdata, 8);
#endif
    debug("\nserial_local_ack opt_code = %d module = %d  ext_addr = %08llX\n",
          serial_msg->opt_code, serial_msg->module_no,  ext_addr);



    switch (serial_msg->opt_code) {
    case 0x00:
        ext_addr = getaddr_byte(serial_msg->pdata, 8);
        //module_no = serial_msg->module_no;
        module_no = serial_msg->pdata[8];
        net_addr = serial_msg->net_addr;
        ret = zigsrch_ext(ext_addr, &pdef);
        if (ret == 0) {      //节点已经存在

            if (pdef.mod_no != module_no || pdef.net_addr != net_addr) {      //需要更新
                debug("\n *******\nline: %d net_addr = %X pdef.net_addr = %X module_no = %d, pdef.mod_no = %d\n*****\n",
                      __LINE__, net_addr, pdef.net_addr, module_no, pdef.mod_no);
                if (zigdev_mod(ext_addr, pdef.area_id, module_no, net_addr, 0) < 0) {
                    log_error(LOG_DEBUG, "\nzig dev mod error happen\n");
                } else {
                    debug("\n\nzig device mod successful\n\n");
                }

            }


            serial_msg = serial_msgform(buffer, 0x02, 0x00, 0x01, ext_addr, 0, net_addr, NULL, 0);

            SerialPacketSend(serial_msg, pserial_des);
            log_error(LOG_NOTICE, "a defined node ack ext_addr = %016llX", ext_addr);
            break;
        } else if (zigadd_un(ext_addr, module_no, net_addr) < 0) {
            log_error(LOG_NOTICE, "zigadd_un perhaps node exist ext_addr = %llX net_addr = %X \
                      mod_no = %d", ext_addr, net_addr, module_no);
            break;
        }

        tcp_msg = serial_to_tcp(buffer, serial_msg);
        //tcp_msg->cmd = tcp_msg->cmd;

        for (i = 0; i < MAX_LINK_SOCK; i++) {
            psock = sttConnSock + i;

            if (psock->fdSock > 0)
                SockPackSend(tcp_msg, psock->fdSock, psock);

        }

        break;
    default:
        break;
    }

    return NULL;

}




static void * serial_local_query(sqlite_db_t * sqlite,  void * conn, void * arg, int size)
{

    return NULL;
}

static void * serial_local_set(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    return NULL;
}

static void * net_local_ack(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    return NULL;
}



int send_all_cb(void * arg, int n_column, char ** column_value, char ** column_name)
{
    char buffer[100];
    uint8_t pdata[20];
    conn_sock_t * psock = arg;
    tcp_msg_t * pmsg = (tcp_msg_t * )(buffer);
    uint64_t ext_addr;
    uint8_t mod;
    uint16_t area_id;
    int retry_cnt = 0;

    /*
    printf("\nn_column = %d\n", n_column);
    printf("\n");
    int i;
    for (i = 0; i < n_column; i++) {
       printf("%s = %s   ", column_name[i], column_value[i]);
    }
    */
    ext_addr = strtouint(column_value[0], 17);
    area_id = atoi(column_value[1]);
    mod = atoi(column_value[3]);

    pdata[0] = (area_id >> 8)&0xFF;
    pdata[1] = area_id & 0xFF;

    pdata[2] = mod;

    tcp_msgform(pmsg, 0, 0, 0x10, ext_addr, pdata, 3);

    if (SockPackSend(pmsg, psock->fdSock, psock) == SOCK_FULL) {
        if (retry_cnt++ < 5)
            usleep(10000);
        else
            return -1;
    }

    //printf("\nxxxxxxxxxxxxxx\n");
    return 0;

}

static inline void send_all_node(conn_sock_t * psock)
{
    zigdev_tra(send_all_cb, psock);
}

/*
static int require_config(conn_sock_t * psock, void * arg, int size)
{
    debug("\nrequire_config\n");
    send_all_node(psock);
    return 0;
}
*/

static void require_unconfig_cb(void * arg, int size, zig_un_t * pun)
{
    uint8_t retry = 5;
    uint8_t buffer[100];
    tcp_msg_t * pmsg = (tcp_msg_t *)buffer;
    conn_sock_t * psock = arg;

    tcp_msgform(pmsg, 0, 0, 0, pun->ext_addr, &pun->mod_no, 1);
    while (SockPackSend(pmsg, psock->fdSock, psock) == SOCK_FULL) {
        usleep(10000);
        log_error(LOG_NOTICE, "require_unconfig_cb");
        //printf("\nerrpr\n");
        retry--;
        if (!retry)
            break;
    }
    return;
}

/*
static int require_unconfig(conn_sock_t * psock, void * arg, int size)
{
    debug("\nrequire unconfig node\n");
//    typedef void (*zigtra_un_cb)(void * arg, int size, zig_un_t * pun);

    zigtra_un(psock, 0, require_unconfig_cb);
    return 0;
}
*/

static void * net_local_query(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    conn_sock_t * psock = conn;
    tcp_msg_t * pmsg = arg;
    debug("\nnet_local_query opt_code = %d\n", pmsg->opt_code);

    switch(pmsg->opt_code) {
    case 0x10:          //请求配置
        send_all_node(psock);
        /*
        if (event_queue_push(EVENT_NORMAL, psock, require_config, NULL, 0) < 0) {
            log_error(LOG_NOTICE, "event_queue_push");
            //exit(1);
        }
        */
        break;
    case 0x01:
        zigtra_un(psock, 0, require_unconfig_cb);
        /*
        if (event_queue_push(EVENT_NORMAL, psock, require_unconfig, NULL, 0) < 0) {
            log_error(LOG_NOTICE, "event_queue_push");
            //exit(1);
        }
        */
        break;
    default:
        log_error(LOG_DEBUG, "net_local_query");
        break;
    }

    return NULL;
}

static void set_module_area(conn_sock_t * psock, tcp_msg_t * pmsg)
{
    serial_msg_t * serial_msg;
    uint8_t mod_no;
    uint16_t net_addr;
    uint8_t buffer[300];
    zig_un_t * pun;

    uint64_t ext_addr = pmsg->ext_addr;
    uint16_t area_id = ((pmsg->pdata[0] << 8) & 0xFF00) + pmsg->pdata[1];


    debug("\nset_module_area file:%s  line:%d\n", __FILE__, __LINE__);

    pun = zigsrch_un(ext_addr);

    if (pun == NULL) {

        //return;
        int ret;
        zig_def_t pdef;
        debug("\nline:%d\n", __LINE__);
        ret = zigsrch_ext(ext_addr, &pdef);
        if (ret == 0) {
            log_error(LOG_NOTICE, "intend to set a node unexist");
            return;
        } else {
            net_addr = pdef.net_addr;
            mod_no = pdef.mod_no;
            debug("\nintend to modify a node\n");
        }
    } else {
        mod_no = pun->mod_no;
        net_addr = pun->net_addr;
    }

    /*
    if (zigdev_mod(ext_addr, area_id, pun->mod_no, pun->net_addr, 0) < 0) {
        log_error(LOG_DEBUG, "zigadd_dev");
        return;
    }
    */


    zigdel_dev(ext_addr);           //删除设备 防止设备已经存在

    if (zigadd_dev(ext_addr, area_id, mod_no, net_addr, 0) < 0) {
        log_error(LOG_DEBUG, "zigadd_dev");
        return;
    }

    serial_msg = serial_msgform(buffer, 0x02, 0x00, 0x01, ext_addr, 0, net_addr, NULL, 0);

    SerialPacketSend(serial_msg, pserial_des);

    write_version(++g_dev_version);

    zigdel_un(ext_addr);

    return;
}

static void * net_local_set(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    conn_sock_t * psock = conn;
    tcp_msg_t * pmsg = arg;
    debug("\n***net_local_set opt_code = %d\n", pmsg->opt_code);
    switch(pmsg->opt_code) {
    case 0x1:               //设置模块
        //write_version(++g_dev_version);
        debug("\n0x1\n");
        set_module_area(psock, pmsg);
        break;
    case 0x10:              //请求节点配制
        break;
    default:
        log_error(LOG_NOTICE, "\nnet_local_set\n");

    }

    return NULL;
}

uint8_t * getstrbytime(long t, uint8_t str[], int n)
{
    struct tm tm;
    if (n < 7)
        return NULL;
    if (localtime_r(&t, &tm) == NULL)
        return NULL;
    str[0] = 20;
    str[1] = tm.tm_year%100;
    str[2] = tm.tm_mon;
    str[3] = tm.tm_mday;
    str[4] = tm.tm_hour;
    str[5] = tm.tm_min;
    str[6] = tm.tm_sec;
    return str;
}

long gettimebystr(uint8_t timer[], int n)
{
    struct tm tm;

    if (n < 7)
        return -1;

    memset(&tm, 0, sizeof(tm));

    tm.tm_isdst = 1;        //夏令时 待定

    tm.tm_year = 100 + timer[1];
    tm.tm_mon = timer[2];
    tm.tm_mday = timer[3];
    tm.tm_hour = timer[4];
    tm.tm_min = timer[5];
    tm.tm_sec = timer[6];

    return mktime(&tm);
}

int send_press_cb(void * arg, int n_column, char ** column_value, char ** column_name)
{
    conn_sock_t * psock = arg;
    uint8_t * pdata;
    float high, low, rate, tem;

    uint8_t buffer[300];
    tcp_msg_t * pmsg = (tcp_msg_t * )buffer;
    long timestamp;

    pmsg->ext_addr = strtouint(column_value[1], 8);

    if (n_column < 6) {
        log_error(LOG_NOTICE, "send_press_cb");
        return 0;
    }
    timestamp      = atol(column_value[0]);
    if (getstrbytime(timestamp, pmsg->pdata, 7) == NULL)
        return -1;

    high = atof(column_value[2]);
    low  = atof(column_value[3]);
    rate = atof(column_value[4]);
    tem  = atof(column_value[5]);
    pdata = pmsg->pdata;


    pdata[7] = (uint8_t)high;
    pdata[8] = (high - (uint8_t) high)*10;


    pdata[9] =  (uint8_t)low;
    pdata[10] = (low - (uint8_t) low)*10;

    pdata[11] = (uint8_t)rate;
    pdata[12] = (rate - (uint8_t) rate)*10;

    pdata[13] = (uint8_t)tem;
    pdata[14] = (tem - (uint8_t) tem)*10;



    tcp_msgform(pmsg, 0x00, 7, 0, pmsg->ext_addr, pdata, 15);


    SockPackSend(pmsg, psock->fdSock, psock);

    return 0;
}

int send_press_data(sqlite_db_t * sqlite ,conn_sock_t * psock, void * arg, int size)
{
    uint8_t * pbuf = arg;
    long      start, end;

    char sql[200];

    start = gettimebystr(pbuf, 7);        //开始时间
    end = gettimebystr(pbuf + 7, 7);      //结束时间

    debug("\nline = %d,  start = %ld, end = %ld\n", __LINE__, start, end);

    sql_format(sql, 300, "select * from %s where time >= %ld and time <= %ld limit 10;", "BLOOD_PRESS",
               start, end);

    //cache_table(sql, send_press_cb, psock);
    smtdb_table(sqlite, sql, send_press_cb, psock);
    return 0;
}

/*          血压计查询                */
static void * net_sphy_query(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{

    conn_sock_t * psock = conn;
    tcp_msg_t * tcp_msg = arg;

    uint8_t  t[14];



    debug("\nnet_sphy ack line= %d\n", __LINE__);

    if (tcp_msg->msg_len < TCPHEADER_LEN + 14) {
        log_error(LOG_NOTICE, "net_sphy_ack");
        return NULL;
    }

    if (tcp_msg->opt_code != 0x00)
        return NULL;

    memcpy(t, tcp_msg->pdata, 14);


    send_press_data(sqlite, psock, t, 14);


    return NULL;
}

/*                              血压计模块应答                   */
static void * serial_sphy_ack(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{

    serial_msg_t * serial_msg = arg;
    zig_def_t pdef;
    int ret;
    float high, low, rate, tem;
    uint64_t ext_addr = 0;
    uint8_t timer_buf[8];
    uint8_t * pdata;
    int tm;
    char ext_buf[30];
    char sql[200];
    uint8_t buffer[SERIAL_BUFFER];

    debug("\n-------sphy_mod_ack line = %d\n", __LINE__);


    if (serial_msg->msg_len < SERIALHEADER_LEN + 15) {
        log_error(LOG_NOTICE, "msg len error need %d and fact %d", SERIALHEADER_LEN + 13, serial_msg->msg_len);
        return NULL;
    }

    ret = zigsrch_net(serial_msg->net_addr, &pdef);

    if (ret == 0) {         //超找到设备
        ext_addr = pdef.ext_addr;
    }

    pdata = serial_msg->pdata;

    memcpy(timer_buf, pdata, 7);

    tm = gettimebystr(timer_buf, 7);

    pdata += 7;
    high = pdata[0]*1.0 + (pdata[1]*1.0/10.0);      //高压
    low  = pdata[2]*1.0 + (pdata[3]*1.0/10.0);     //低压
    rate   = pdata[4]*1.0 + (pdata[5]*1.0/10.0);   //心率
    tem   = pdata[6]*1.0 + (pdata[7]*1.0/10.0);    //体温


    uinttostr(ext_addr, ext_buf, 30);
    sql_format(sql, 200, "insert into BLOOD_PRESS values(%d, '%s', %f, %f, %f, %f)",
               tm, ext_buf, high, low, rate, tem);
    /*
    if (cache_opt_item(TBL_BLOOD, sql) < 0) {
        log_error(LOG_NOTICE, "cache_opt_item");
    }
    */
    if (smtdb_opt(sqlite, sql) < 0) {
        log_error(LOG_NOTICE, "cache_opt_item");
    }
    serial_msg = serial_msgform(buffer, 0x02, 7, 0, ext_addr, 0, serial_msg->net_addr, NULL, 0);
    SerialPacketSend(serial_msg, pserial_des);

    return NULL;
}


static serial_msg_t * tcp_serial(void * buffer, tcp_msg_t * tcp_msg, int addr, void * data, int len)
{
    serial_msg_t * s;
    zig_def_t    pdef;
    int ret;
    uint16_t net_addr;
    int size = tcp_msg->msg_len - TCPHEADER_LEN;
    uint8_t * pdata = tcp_msg->pdata;

    if (data != NULL) {
        size = len;
        pdata = data;
    }

    ret = zigsrch_ext(tcp_msg->ext_addr, &pdef);



    if (ret < 0) {
        log_error(LOG_NOTICE, "no such device");
        net_addr = 0;

        if (net_addr == 0) {      //终端向未配置的节点发送
            zig_un_t * pun = zigsrch_un(tcp_msg->ext_addr);
            if (pun != NULL) {
                net_addr = pun->net_addr;
                printf("\nnet_addr = %X\n", net_addr);
            }
           // net_addr = 0;
        }


    } else {
        net_addr = pdef.net_addr;
        debug("\ndst net_addr = %d ext_addr = %llX tcp_msg->ext_addr = %llx\n",
              net_addr, pdef.ext_addr, tcp_msg->ext_addr);
    }



    s = serial_msgform(buffer, tcp_msg->cmd, tcp_msg->module_no,
                       tcp_msg->opt_code, tcp_msg->ext_addr, addr,net_addr,
                       pdata, size);

#ifdef DEBUG_MODULES
    printf("\n");
    int i;

    for (i = 0; i < s->msg_len; i++) {
        printf("%x  ", ((uint8_t *)s)[i]);
    }

    printf("\n");
#endif
    return s;
}


static int send_ir_code(const uint8_t * pvalue, const uint16_t value_cnt, tcp_msg_t * tcp_msg,
                            uint8_t addr)
{
#pragma pack(1)
    struct ir_msg {
        uint8_t serial;
        uint8_t seq;
        uint16_t len;
        uint8_t pdata[0];
    };
#pragma pack()
    serial_msg_t * pmsg;
    //uint8_t buffer[MAX_IR_BUF + SERIALHEADER_LEN * 10];
    uint8_t buffer[MAX_IR_BUF + SERIALHEADER_LEN];
    struct ir_msg * ir_msg;
    int i;
    int total_sent = 0;
    int this_sent = 0;
    int all_sent = 0;
    int ret;
    uint64_t ext_addr;
    uint16_t net_addr;
    uint8_t serial;
    uint8_t * pdata = buffer;

    zig_def_t    pdef;

    if (value_cnt > MAX_IR_BUF) {
        log_error(LOG_NOTICE, "send_ir_code");
        return 0;
    }

    serial = ++ir_serial;

    ret = zigsrch_ext(tcp_msg->ext_addr, &pdef);
    if (ret < 0) {
        log_error(LOG_NOTICE, "no such zigbee to ir mod");
        return -1;
    }

    ext_addr = pdef.ext_addr;
    net_addr = pdef.net_addr;

    for (i = 0;total_sent < value_cnt; i++) {

        pmsg = (serial_msg_t *)pdata;

        ir_msg = (struct ir_msg *)(pmsg->pdata);
        ir_msg->serial = serial;
        ir_msg->seq = i + 1;
        ir_msg->len = htons(value_cnt);


        this_sent = (value_cnt - total_sent > 60) ? 60 : (value_cnt - total_sent);
        memcpy(ir_msg->pdata, pvalue + total_sent,  this_sent);
        total_sent += this_sent;

        pmsg = serial_msgform(pdata, SET, 8, tcp_msg->opt_code, ext_addr, addr, net_addr,
                              (uint8_t*)ir_msg, this_sent + sizeof(*ir_msg));
        //pdata += pmsg->msg_len;

        all_sent += pmsg->msg_len;

        if (SerialPacketSend(pmsg, pserial_des) < 0) {
            usleep(20000);
            SerialPacketSend(pmsg, pserial_des);
        }
        usleep(100000);         //100ms的延迟~~~~~！！！！！！   协调器来不及处理了
    }

    /*
    if (serial_buffer_send(pserial_des, buffer, all_sent) < 0) {
        usleep(20000);
        serial_buffer_send(pserial_des, buffer, all_sent);
    }
    */
    return 0;
}

/*             发送红外控制信号         */
static int ir_emits(sqlite_db_t * sqlite, conn_sock_t * psock, void * arg, int size)
{
    tcp_msg_t * tcp_msg = arg;
    sqlite3_stmt  * pstmt;
    sqlite3 * pdb;
    uint8_t key_type, key_word;
    uint16_t key_code;
    int ret;
    char sql[100];
    const void * pvalue = NULL;
    int value_cnt;

    debug("\nir_emit_cb line = %d msg_len = %d\n", __LINE__, tcp_msg->msg_len);

    key_type = tcp_msg->pdata[0];
    key_word = tcp_msg->pdata[1];
    key_code = (tcp_msg->pdata[2] << 8) + tcp_msg->pdata[3];

    debug("\nkey_type = %d, key_word = %d, key_code = %d   line = %d\n", key_type, key_word, key_code, __LINE__);

    sql_format(sql, 100, "select * from ir_key where ir_key_type = %d and ir_key_word = %d and ir_key_code = %d;",
               key_type, key_word, key_code);

    pdb = sqlite->sqlite_db;

    ret = sqlite3_prepare_v2(pdb,
                       sql,
                       -1, &pstmt, NULL );
    if (ret != SQLITE_OK) {
        log_error(LOG_NOTICE, sqlite3_errmsg(pdb));
        cache_release_db();
        return -1;
    }

    ret = sqlite3_step(pstmt);

    if (ret == SQLITE_ROW) {

        pvalue = sqlite3_column_blob(pstmt, 3);
        value_cnt = sqlite3_column_bytes(pstmt, 3);
        debug("\nfind a code line= %d  value_cnt = %d\n", __LINE__, value_cnt);
        if (pvalue != NULL) {
            send_ir_code(pvalue, value_cnt, tcp_msg, psock->addr);
        }
    } else if (ret == SQLITE_DONE) {        //完成了  未学习?
        uint8_t buffer[100];
        tcp_msg_t * pmsg;
        uint8_t data = 0xFF;
        log_error(LOG_NOTICE, "unexpected sqlite_done");
        pmsg = tcp_msgform(buffer, ACK, 8, tcp_msg->opt_code, tcp_msg->ext_addr, &data, 1);
        SockPackSend(pmsg, psock->fdSock, psock);
    }

    sqlite3_finalize(pstmt);

    return 0;
}



/*             zigbee转红外模块设置                                         */
static void * net_irmod_set(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    conn_sock_t * psock = conn;
    serial_des_t * pserial = pserial_des;
    tcp_msg_t * tcp_msg = arg;
    serial_msg_t * serial_msg = NULL;
    uint8_t msg_buffer[200];
    uint8_t pdata[50];

    int len;

    len = get_tcp_len(tcp_msg);

    if (len > 30) {
        log_error(LOG_NOTICE, "msg too long");
        return NULL;
    }

    switch (tcp_msg->opt_code) {
    case 0x00:                  //红外学习

        memcpy(pdata, tcp_msg->pdata, len);
        pdata[len] = ++ir_serial;
        serial_msg = tcp_serial(msg_buffer, tcp_msg, psock->addr, pdata, len+1);

        psock->ir_buffer.ir_cnt = 0;
        psock->ir_buffer.serial = pdata[len];
        memset(psock->ir_buffer.seq_received, 0, MAX_IR_SEQ);
        psock->ir_buffer.last_seq = 0;          //上次收到的序列号
        psock->ir_buffer.key_type = serial_msg->pdata[0];
        psock->ir_buffer.key_word = serial_msg->pdata[1];
        psock->ir_buffer.key_code = (serial_msg->pdata[2] << 8) + serial_msg->pdata[3];


        SerialPacketSend(serial_msg, pserial);

        debug("\nir_serial = %d line:%d\n", ir_serial, __LINE__);
        break;

    case 0x01:                  //红外控制
        ir_emits(sqlite, psock,  tcp_msg, tcp_msg->msg_len);

        break;
    default:
        log_error(LOG_NOTICE, "net_irmod_set");
    }

    return NULL;
}

/*           zigbee转红外模块查询                               */
static void * net_irmod_query(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    log_error(LOG_NOTICE, "receove a net_irmod_query");
    return NULL;
}


static void restore_ir(sqlite_db_t * sqlite, conn_sock_t * psock, serial_msg_t * serial_msg, uint64_t ext_addr)
{
    char sql[200];
    uint8_t ack_buf[200];
    sqlite3 * pdb;
    sqlite3_stmt * pstmt;
    uint8_t data;
    uint8_t key_type, key_word;
    uint16_t key_code;
    uint8_t version = 0;
    int ret;

    key_type = psock->ir_buffer.key_type;
    key_word = psock->ir_buffer.key_word;
    key_code = psock->ir_buffer.key_code;

    sql_format(sql, 200, "delete from ir_key where key_type = %d and key_word = %d and \
               key_code = %d;", key_type, key_word, key_code);

    cache_opt_item(TBL_IR_KEY, sql);

    sql_format(sql, 200, "insert into ir_key values(%d, %d, %d, ?, %d);",
               key_type, key_word, key_code, version);


    pdb = sqlite->sqlite_db;

    ret = sqlite3_prepare_v2(pdb,
                       sql,
                       -1, &pstmt, NULL );
    if (ret != SQLITE_OK) {
        log_error(LOG_NOTICE, sqlite3_errmsg(pdb));
        cache_release_db();
        return;
    }
    sqlite3_bind_blob(pstmt, 1, psock->ir_buffer.ir_buffer, psock->ir_buffer.ir_cnt, NULL);

    ret = sqlite3_step(pstmt);

    if (ret == SQLITE_DONE) {
        debug("\nsqlite store irkey successful line = %d\n", __LINE__);
        data = 0x00;
        tcp_msgform(ack_buf, ACK, serial_msg->module_no, serial_msg->opt_code, ext_addr,  &data, 1);   //0地址
        if (SockPackSend((tcp_msg_t *)ack_buf, psock->fdSock, psock) < 0) {
            usleep(20000);
            SockPackSend((tcp_msg_t *)ack_buf, psock->fdSock, psock);
        }
    } else {
        log_error(LOG_NOTICE, "sqlite store ir key fail");
        data = 0xFF;
        tcp_msgform(ack_buf, ACK, serial_msg->module_no, serial_msg->opt_code, ext_addr,  &data, 1);
        SockPackSend((tcp_msg_t *)ack_buf, psock->fdSock, psock);
    }

    sqlite3_finalize(pstmt);

    return;
}

/*          红外学习                */
static int ir_study(sqlite_db_t * sqlite, conn_sock_t * psock, void * arg, int size)
{
    serial_msg_t * serial_msg = arg;
    uint8_t key_type, key_word, serial, seq;
    uint16_t key_code, len;
    uint16_t dat_len;
    int ret;
    zig_def_t pdef;
    debug("\nir_study_cb line = %d msg_len = %d seq = %d\n", __LINE__,
          serial_msg->msg_len, serial_msg->pdata[4]);

    ret = zigsrch_net(serial_msg->net_addr, &pdef);

    if (ret < 0) {
        log_error(LOG_NOTICE, "\nno such device\n");
    }


    key_type = serial_msg->pdata[0];
    key_word = serial_msg->pdata[1];
    key_code = (serial_msg->pdata[2] << 8) + serial_msg->pdata[3];
    serial = serial_msg->pdata[4];
    seq = serial_msg->pdata[5];
    len = (serial_msg->pdata[6] << 8) + serial_msg->pdata[7];
    dat_len = get_serial_len(serial_msg) - 8;

    if (seq >= MAX_IR_SEQ)
        return -1;

    debug("\nkey_type = %d, key_word = %d, key_code = %d, serial = %d, seq = %d, dat_len = %d\
          msg_len = %d\n",
          key_type, key_word, key_code, serial, seq, dat_len, serial_msg->msg_len);

    if (psock->ir_buffer.key_type != key_type || psock->ir_buffer.key_word != key_word ||
            psock->ir_buffer.key_code != key_code || psock->ir_buffer.serial != serial) {
        log_error(LOG_NOTICE, "message receive error");
        return -1;
    }

    if (psock->ir_buffer.seq_received[seq] == 1) {      //数据已经接收
        return -1;
    }

    //memcpy(psock->ir_buffer.ir_buffer + psock->ir_buffer.ir_cnt, serial_msg->pdata + 8, dat_len);
    memcpy(psock->ir_buffer.ir_buffer + 60 * (seq - 1), serial_msg->pdata + 8, dat_len);    //防止数据包乱序出错
    psock->ir_buffer.ir_cnt += dat_len;
    psock->ir_buffer.seq_received[seq] = 1;




    if (psock->ir_buffer.ir_cnt == len) {   //接收完成

        debug("\nnow receive a key type = %d, word = %d, code = %d\n", key_type, key_word, key_code);
        //数据存入数据库中
        restore_ir(sqlite, psock, serial_msg, pdef.ext_addr);

    } else {

        if (seq - psock->ir_buffer.last_seq > 1) {      //数据帧没有按序发送
            //请求重传没有按序到达的数据帧
            log_error(LOG_NOTICE, "message aint sort");

        }
    }

    return 0;
}

/*                              zigbee 转红外模块应答         */
static void * serial_irmod_ack(sqlite_db_t * sqlite, void * conn, void * arg, int size)
{
    serial_msg_t * serial_msg = arg;
    conn_sock_t * psock = NULL;
    uint8_t innet_addr = serial_msg->addr;

    if (innet_addr == 0 || innet_addr > MAX_LINK_SOCK) {
        log_error(LOG_NOTICE, "addr not exist");
        return NULL;
    }

    psock = &sttConnSock[innet_addr-1];

    switch (serial_msg->opt_code) {
    case 0x00:      //红外学习返回
        ir_study(sqlite, psock, serial_msg, serial_msg->msg_len);
\
        break;
    case 0x01: //红外控制命令返回  暂时 直接透传
        serial_mod_ack(NULL, psock, serial_msg, size);
        break;
    default:
        log_error(LOG_NOTICE, "serial_irmod_ack");
    }

    return NULL;
}


const zig_mod_t   g_zig_mod[255] =
{
    {
        0,
        EVENT_L0,
        {net_local_ack, net_local_query, net_local_set},
        {serial_local_ack, serial_local_query, serial_local_set}
    },

    {
        1,              //模块1  窗帘
        EVENT_EMSG,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}

    },

    {
        2,              //模块2  温湿度
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },

    {
        3,             //模块3  光照传感器
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },

    {
        4,              //模块4 有害气体
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        5,              //机械手
        EVENT_EMSG,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        6,              //窗磁门磁
        EVENT_EMSG,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        7,              //血压计
        EVENT_NORMAL,
        {net_mod_ack, net_sphy_query, net_mod_set},
        {serial_sphy_ack, serial_mod_query, serial_mod_set}
    },
    {
        8,              //zigbee 转红外模块
        EVENT_RLTM,
        {net_mod_ack, net_irmod_query, net_irmod_set},
        {serial_irmod_ack, serial_mod_query, serial_mod_set}
    },
    {
        9,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        10,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        11,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        12,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        13,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        14,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        15,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        16,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        17,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        18,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        19,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },
    {
        20,              //窗磁门磁
        EVENT_NORMAL,
        {net_mod_ack, net_mod_query, net_mod_set},
        {serial_mod_ack, serial_mod_query, serial_mod_set}
    },


};
