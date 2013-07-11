#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#define     TCPHEADER_LEN     (sizeof(tcp_msg_t)+1)
#define     SERIALHEADER_LEN    (sizeof(serial_msg_t)+1)

#define     END_POINT 20

/*       TCP 包          */
#pragma pack(1)             //字节对齐为1字节
typedef struct tcp_msg {
    uint32_t    head;
    uint8_t     cmd;
    uint16_t    msg_len;
    uint8_t     module_no;
    uint8_t     opt_code;
    uint64_t    ext_addr;
    uint8_t     pdata[0];
} tcp_msg_t;

/*       串口包           */

typedef struct {
    uint16_t   head;
    uint8_t    cmd;
    uint8_t    msg_len;
    uint8_t    module_no;
    uint8_t    opt_code;
    uint8_t    addr;
 //   uint8_t    end_point;
    uint16_t   net_addr;
    union {
        uint64_t   ext_addr;            //发送时捎带的地址
        uint8_t    bytes[8];            //接收时使用 byte[0] LRI byte[1] LRI byte[3] byte[4] cwd
        struct {
            uint8_t lri;        //链路质量
            int8_t  rssi;
            uint8_t resv[6];        //暂无
        } info;
    } resv;
    uint8_t    pdata[0];
} serial_msg_t;



#pragma pack()


uint32_t cal_check(uint8_t *value, uint32_t size);

static inline tcp_msg_t * tcp_msgform(void * buffer, uint8_t cmd,
                uint8_t module_no,
                uint8_t opt_code,
                uint64_t ext_addr,
                uint8_t * pdata,
                uint16_t size)
{
    int len = TCPHEADER_LEN + size;
    tcp_msg_t * pmsg = buffer;
    pmsg->head = htonl(0x2A2F2A2F);
    pmsg->cmd = cmd;
    pmsg->msg_len = htons(len);
    pmsg->module_no = module_no;
    pmsg->opt_code = opt_code;
    pmsg->ext_addr = ext_addr;

    memcpy(pmsg->pdata, pdata, size);

    ((uint8_t *)buffer)[len-1] = cal_check(buffer, len-1);


    return pmsg;
}

/*       得到数据长度           */
static inline int get_tcp_len(tcp_msg_t * pmsg)
{
    return pmsg->msg_len - TCPHEADER_LEN;
}

static inline serial_msg_t * serial_msgform(void * buffer, uint8_t cmd,
                   uint8_t module_no,
                   uint8_t opt_code,
                   uint64_t ext_addr,
                   uint8_t innet_addr,
                   uint16_t net_addr,
                   uint8_t *pdata,
                   uint16_t size)
{
    serial_msg_t * pmsg = buffer;
    pmsg->head = htons(0x2A2F);
    pmsg->cmd = cmd;
    pmsg->msg_len = SERIALHEADER_LEN + size;
    pmsg->module_no = module_no;
    pmsg->opt_code = opt_code;
    pmsg->addr = innet_addr;
    pmsg->net_addr =  net_addr;
    pmsg->resv.ext_addr = ext_addr;
//    pmsg->end_point = END_POINT;
    memcpy(pmsg->pdata, pdata, size);
    ((uint8_t *)buffer)[pmsg->msg_len-1] = cal_check(buffer, pmsg->msg_len-1);

//    printf("\nmsg_len = %d\n", pmsg->msg_len);
    return pmsg;
}


static inline int get_serial_len(serial_msg_t * pmsg)
{
    return pmsg->msg_len - SERIALHEADER_LEN;
}


uint64_t getaddr_byte(uint8_t * pdata, int len);






#endif // PROTOCOL_H
