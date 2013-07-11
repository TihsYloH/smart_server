#ifndef ZIGBEE_DEV_H
#define ZIGBEE_DEV_H

#include <stdint.h>
#include "tcp_server.h"
#include "list.h"
#include "rbtree.h"
#include "sqlite_db.h"

#define     MAX_ZIGBEE_DEVICE   200

#define     MAX_NAME_LEN        30


typedef struct zig_un {
    struct list_head entry;
    uint64_t ext_addr;
    uint16_t net_addr;
    uint8_t mod_no;
} zig_un_t;


typedef struct zig_def {
    struct rb_node node;
    uint64_t ext_addr;
    uint16_t area_id;
    uint16_t net_addr;
    uint16_t mod_no;
    uint16_t innet_addr;
} zig_def_t;


static inline void zigset_def(zig_def_t * pdef, uint16_t area_id,
                       uint16_t net_addr, uint16_t mod_no, uint16_t innet_addr)
{
    pdef->area_id = area_id;
    pdef->net_addr = net_addr;
    pdef->mod_no = mod_no;
    pdef->innet_addr = innet_addr;
}

static inline void zigset_un(zig_un_t * pun, uint16_t net_addr, uint8_t mod_no)
{
    pun->net_addr = net_addr;
    pun->mod_no = mod_no;
}


/*                字符串转化为64位整数                      */
uint64_t strtouint(const char * buffer, int n);
/*                64位整数转化为字符串                      */
void uinttostr(uint64_t ext_addr, char * str, int n);



typedef void (*zigtra_un_cb)(void * arg, int size, zig_un_t * pun);


/*     添加未配置节点             */
int zigadd_un(uint64_t ext_addr, uint8_t mod_no, uint16_t net_addr);
/*      删除未配置节点               */
int zigdel_un(uint64_t ext_addr);
/*      查找未配置节点                  */
zig_un_t * zigsrch_un(uint64_t ext_addr);

zig_un_t * zigsrch_unnet(uint16_t net_addr);

zig_un_t * zigtra_un(void * arg, int size,  zigtra_un_cb cb);


long write_version(long version);
/*         返回配制版本号       */
long zigdev_version(void);
/*      节点数量        */
int zigcon_cnt(void);

int zigdev_init(void);
/*         添加一个zigbee 设备                     */
int zigadd_dev(uint64_t ext_addr, uint16_t area_id, uint8_t mod_no, uint16_t net_addr, uint16_t innet_addr);
/*         删除一个zigbee设备         */
int zigdel_dev(uint64_t ext_addr);

/*         查询zigbee设备                         */

/*        查找到返回值大于0并且设置   zig_def_t       */
int zigsrch_net(uint16_t net_addr,zig_def_t *);
int zigsrch_ext(uint64_t ext_addr,zig_def_t *);

/*         改变一个zigbee设备                         */
int zigdev_mod(uint64_t ext_addr, uint16_t area_id, uint8_t mod_no, uint16_t net_addr, uint16_t innet_addr);

int zigdev_tra(sqlite_cb cb, void * arg);

























/*
int zigadd_def(uint64_t ext_addr, uint16_t area_id, uint8_t mod_no, uint16_t net_addr, uint16_t innet_addr);
int zigdel_def(uint64_t ext_addr);
zig_def_t * zigsrch_def(uint64_t ext_addr);

zig_def_t * zigsrch_bynet(uint16_t net_addr);


*/




#endif // ZIGBEE_DEV_H
