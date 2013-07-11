#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "sqlite_db.h"
#include "log.h"
#include "zigbee_dev.h"
#include "tcp_server.h"
#include "list.h"
#include "sqlite3.h"
#include "sqlite_db.h"
#include "list.h"
#include "buffer.h"
#include "iic.h"

#define MAX_OPT_CODES       20


//moudle name "01"  "02"  "03"

#define __ZIG_THREAD_LOCK
#ifdef __ZIG_THREAD_LOCK


#define xDEBUG_ZIGDEV

#ifdef DEBUG_ZIGDEV
#define debug_dev(format,args...) do {printf(format, ##args);fflush(stdout); } while (0)
#else
#define debug_dev(format,args...)
#endif

#define ZIG_LOCK(x)      pthread_mutex_lock(&((x)->lock));
#define ZIG_UNLOCK(x)    pthread_mutex_unlock(&((x)->lock));
#else
#define ZIG_LOCK(x)
#define ZIG_UNLOCK(x)
#endif

#define   SQLITE_CONFIG_PATH        "/mnt/config"
#define   REMAP_CONFIG_PATH         "/tmp/tmp_config"

struct zigun_set {
    struct list_head head;
    pthread_mutex_t lock;
} zigun_set;


struct zigun_set * punset;



int zigun_init(void)
{
    punset = &zigun_set;
    INIT_LIST_HEAD(&punset->head);
    pthread_mutex_init(&punset->lock, NULL);

    return 0;
}

int zigdef_init(void);

static int creat_config(sqlite_db_t * sqlite_db)
{
    char * sql;

    sql = "create table config(ext_addr nvarchar(32) primary key,\
                              area_id integer not null,\
                              net_addr integer not null, \
                              mod_no integer not null,\
                              innet_addr integer NOT NULL\
                              );";
    if (sql_process(sqlite_db, sql, NULL, NULL) < 0) {
            log_error(LOG_EMERG, "create config error");
        return -1;
    }
    return 0;
}
/******************************/
typedef struct remap_s {
    sqlite_db_t  * sd_db;
    sqlite_db_t  * ram_db;
    pthread_mutex_t lock;
} remap_t;

static remap_t * premap = NULL;
uint64_t strtouint(const char * buffer, int n);

static int ram_print(void * db, int n_column, char ** column_value, char ** column_name)
{
    int i;

    printf("\nn_column = %d\n", n_column);
    printf("\n");
    for (i = 0; i < n_column; i++) {
       printf("%s = %s   ", column_name[i], column_value[i]);
    }


    zig_def_t * pdef = db;

    if (n_column != 5) {
        log_error(LOG_EMERG, "ext_srch_cb");
        return -1;
    }

    pdef->ext_addr = strtouint(column_value[0], 30);
    pdef->area_id = atoi(column_value[1]);
    pdef->net_addr = atoi(column_value[2]);
    pdef->mod_no = atoi(column_value[3]);
    pdef->innet_addr = atoi(column_value[4]);

    char * ptr;

    ptr = (char *)(&pdef->ext_addr);
    for (i = 0; i < 8; i++)
        printf("%02x", ptr[i]);

    printf("\n");
    return 0;

}


static int remap_init(void)
{
    int ret;

    if (premap == NULL) {
        premap = malloc(sizeof(*premap));
    }


    pthread_mutex_init(&premap->lock, NULL);

    premap->sd_db = malloc(sizeof(sqlite_db_t));
    premap->ram_db = malloc(sizeof(sqlite_db_t));
    if (premap->sd_db == NULL || premap->ram_db == NULL) {
        log_error(LOG_NOTICE, "malloc");
        return -1;
    }
    ret = sqlite3_open(SQLITE_CONFIG_PATH, &premap->sd_db->sqlite_db);

    if (ret != SQLITE_OK) {
        log_error(LOG_NOTICE, "sqlite3 open error");
        if (ret == SQLITE_ERROR) {
            //³ö´í´¦Àí
            unlink(SQLITE_CONFIG_PATH);
            ret = sqlite3_open(SQLITE_CONFIG_PATH, &premap->sd_db->sqlite_db);
            if (ret == SQLITE_OK) {
                if (creat_config(premap->sd_db) < 0)
                    ret = SQLITE_OK - 1;
            }
        }
        if (ret != SQLITE_OK)
            return -1;
    } else
        creat_config(premap->sd_db);

    char buffer[200];
    snprintf(buffer, 200, "cp %s %s", SQLITE_CONFIG_PATH, REMAP_CONFIG_PATH);
    system(buffer);

    ret = sqlite3_open(SQLITE_CONFIG_PATH, &premap->ram_db->sqlite_db);

    if (ret != SQLITE_OK) {
        log_error(LOG_EMERG, "sqlite3_open");
        return -1;
    }
    zig_def_t def;
    char * sqls = "select * from config;";

    sql_process(premap->ram_db, sqls, ram_print, &def);
    return 0;
}

long write_version(long version)
{
    char * p;
    p = (char *)(&version);

    write_at24c02b(ZIGDEV_VERSION+1, p[0]);
    write_at24c02b(ZIGDEV_VERSION+2, p[1]);
    write_at24c02b(ZIGDEV_VERSION+3, p[2]);
    write_at24c02b(ZIGDEV_VERSION+4, p[0]);
    g_dev_version = version;
    return version;
}

long zigdev_version(void)
{
    char * p;
    long ver;
    p = (char *)(&ver);


    p[0] = read_at24c02b(ZIGDEV_VERSION+1);
    p[1] = read_at24c02b(ZIGDEV_VERSION+2);
    p[2] = read_at24c02b(ZIGDEV_VERSION+3);
    p[3] = read_at24c02b(ZIGDEV_VERSION+4);
    return ver;
}


int zigdev_init(void)
{
    zigun_init();

    if (remap_init() < 0)
        return -1;

    if (read_at24c02b(ZIGDEV_VERSION) != 0x88) {

        g_dev_version = 0;
        write_at24c02b(ZIGDEV_VERSION, 0x88);
        write_version(g_dev_version);
    } else {
        g_dev_version = zigdev_version();
    }

    return 0;
}

int zigcon_cnt(void)
{
    return sql_tblcnt(premap->ram_db, "config");
}

int zigadd_un(uint64_t ext_addr, uint8_t mod_no, uint16_t net_addr)
{
    zig_un_t * pzig;
    zig_un_t * ptmp;
    int ret = 0;
    //printf("\nZIG_LOCK\n");
    ZIG_LOCK(punset);
    //printf("\nZIG_LOCK OBTAIN\n");
    list_for_each_entry_safe(pzig, ptmp, &punset->head, entry) {

        if (pzig->ext_addr == ext_addr) {
            ret = -1;
            pzig->mod_no = mod_no;
            pzig->net_addr = net_addr;
            ZIG_UNLOCK(punset);
            return ret;
        }
    }
    do {
        if (ret == 0) {
            ptmp = malloc(sizeof(*ptmp));
            if (ptmp == NULL) {
                log_error(LOG_NOTICE, "malloc");
                ret = -1;
                break;
            }
            ptmp->ext_addr = ext_addr;
            zigset_un(ptmp, net_addr, mod_no);
            list_add(&ptmp->entry, &punset->head);
            ret = 0;
            break;
        }
    } while (0);

    ZIG_UNLOCK(punset);
    return ret;
}

int zigdel_un(uint64_t ext_addr)
{
    zig_un_t * pzig;
    zig_un_t * ptmp;
    int ret = -1;

    ZIG_LOCK(punset);

    list_for_each_entry_safe(pzig, ptmp, &punset->head, entry) {

        if (pzig->ext_addr == ext_addr) {
            ret = 0;
            list_del(&pzig->entry);
            free(pzig);
            break;
        }
    }
    ZIG_UNLOCK(punset);
    return ret;
}

zig_un_t * zigtra_un(void * arg, int size, zigtra_un_cb cb)
{
    zig_un_t * pzig;
    zig_un_t * ptmp;



    ZIG_LOCK(punset);

    list_for_each_entry_safe(pzig, ptmp, &punset->head, entry) {
        cb(arg, size, pzig);
        //sleep(1);
    }


    ZIG_UNLOCK(punset);
    return NULL;
}

zig_un_t * zigsrch_un(uint64_t ext_addr)
{
    zig_un_t * pzig;
    zig_un_t * ptmp;
    int ret = -1;

    ZIG_LOCK(punset);

    list_for_each_entry_safe(pzig, ptmp, &punset->head, entry) {

        if (pzig->ext_addr == ext_addr) {
            ret = 0;
            break;
        }
    }
    ZIG_UNLOCK(punset);

    if (ret == 0)
        return pzig;
    return NULL;
}

zig_un_t * zigsrch_unnet(uint16_t net_addr)
{
    zig_un_t * pzig;
    zig_un_t * ptmp;
    int ret = -1;

    ZIG_LOCK(punset);

    list_for_each_entry_safe(pzig, ptmp, &punset->head, entry) {

        if (pzig->net_addr == net_addr) {
            ret = 0;
            break;
        }
    }
    ZIG_UNLOCK(punset);

    if (ret == 0)
        return pzig;
    return NULL;
}

static inline void make_char(char * buf)
{
    if (*buf < 10)
        *buf += '0';
    else
        *buf = *buf - 10 + 'A';
}

static inline char make_uint(char  buf)
{
    if (buf >= '0' && buf <= '9')
        buf -= '0';

    else if (buf >= 'A' && buf <= 'F')
        buf = buf - 'A' + 10;
    else {
        printf("\nstr to uint\n");
        buf = 0;
    }
    return buf;
}

void uinttostr(uint64_t ext_addr, char * str, int n)
{
    /*
    char * ptr = (char *)(&ext_addr);
    int i;

    for (i = 0; i < 16; i += 2) {
        str[i] = ((ptr[i>>1]<<4)&0x0F);
        str[i+1] = ((ptr[(i>>1) + 1]&0x0F));

        make_char(str+i);
        make_char(str+i+1);
    }
    str[16] = 0;
*/
    int i;
    uint16_t * ptr = (uint16_t * )str;
    uint16_t back[8];
    snprintf(str, 30, "%016llX", ext_addr);
   // printf("\nsss    %s\n", str);
    for (i = 0; i < 8; i ++) {
        back[7-i] = ptr[i];

    }
    memcpy(str, back, 16);
    str[16] = 0;
  //  printf("\nsss    %s\n", str);

    return;
}

uint64_t strtouint(const char * buffer, int n)
{
    uint64_t ext = 0;
    char * ptr = (char *)(&ext);
    int i;

    for (i = 0; i < 16; i+= 2) {
        ptr[i>>1] = ((make_uint(buffer[i]) << 4) & 0xF0);
        ptr[i>>1] += ((make_uint(buffer[i+1])) & 0x0F);
        //printf("\n%x\n", ptr[i]);

    }
    return ext;
}




int zigadd_dev(uint64_t ext_addr, uint16_t area_id, uint8_t mod_no, uint16_t net_addr, uint16_t innet_addr)
{
    char str[30];
    char sql[100];

    int ret = 0;
    uinttostr(ext_addr, str, 30);

    debug_dev("\nline:%d,   ext = %s\n", __LINE__, str);

    snprintf(sql, 100, "insert into config values('%s', %d, %d, %d, %d);",
             str, area_id, net_addr, mod_no, innet_addr);

    ZIG_LOCK(premap);




    if (sql_process(premap->sd_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }

    if (sql_process(premap->ram_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }
    ZIG_UNLOCK(premap);
    return ret;
}


int zigdel_dev(uint64_t ext_addr)
{
    char str[30];
    char sql[100];
    int ret = 0;

    uinttostr(ext_addr, str, 30);
    debug_dev("\nline:%d,   ext = %s\n", __LINE__, str);

    snprintf(sql, 100, "delete from config where ext_addr = '%s';", str);
    ZIG_LOCK(premap);

    if (sql_process(premap->sd_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }

    if (sql_process(premap->ram_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }
    ZIG_UNLOCK(premap);
    return ret;
}


static int ext_srch_cb(void * arg, int n_column, char ** column_value, char ** column_name)
{
    zig_def_t * pdef = arg;

    debug_dev("\next_srch_cb\n");
    if (n_column != 5) {
        log_error(LOG_EMERG, "ext_srch_cb");
        return -1;
    }

    pdef->ext_addr = strtouint(column_value[0], 30);

    debug_dev("\ncolumn_value[0] = %s\n", column_value[0]);

    pdef->area_id = atoi(column_value[1]);
    pdef->net_addr = atoi(column_value[2]);
    pdef->mod_no = atoi(column_value[3]);
    pdef->innet_addr = atoi(column_value[4]);
    return 0;

}

int zigsrch_ext(uint64_t ext_addr, zig_def_t * pdef)
{


    char str[30];
    char sql[100];

    int ret = 0;
    uinttostr(ext_addr, str, 30);

    debug_dev("\nline:%d,   ext = %s\n", __LINE__, str);


    snprintf(sql, 100, "select * from config where ext_addr = '%s';",
             str);

    pdef->ext_addr = 0;
    ZIG_LOCK(premap);

    if (sql_process(premap->ram_db, sql, ext_srch_cb, pdef) < 0)
        ret = -1;

    ZIG_UNLOCK(premap);
    /*
    zig_def_t def;
    char * sqls = "select * from config;";

    sql_process(premap->ram_db, sqls, ram_print, &def);
    */
    if (pdef->ext_addr != 0)
        ret = 0;
    else
        ret = -1;
    return ret;
}

int zigsrch_net(uint16_t net_addr, zig_def_t * pdef)
{

    char sql[100];

    int ret = 0;

    pdef->ext_addr = 0;

    snprintf(sql, 100, "select * from config where net_addr = %d;",
             net_addr);

    ZIG_LOCK(premap);
    if (sql_process(premap->ram_db, sql, ext_srch_cb, pdef) < 0)
        ret = -1;
    ZIG_UNLOCK(premap);

    if (pdef->ext_addr == 0)
        ret = -1;

    return ret;
}

int zigdev_mod(uint64_t ext_addr, uint16_t area_id, uint8_t mod_no, uint16_t net_addr, uint16_t innet_addr)
{
    zig_def_t def;
    char sql[200];
    int ret;


    if (zigsrch_net(net_addr, &def) == 0) {
        sql_format(sql, 200, "update config set net_addr = %d where net_addr = %d", 0, net_addr);
        sql_process(premap->ram_db, sql, NULL, NULL);
        sql_process(premap->sd_db, sql, NULL, NULL);
    }

    if (zigdel_dev(ext_addr) < 0) {
        log_error(LOG_EMERG, "zigdel_dev");
        ret = -1;
    }

    if (zigadd_dev(ext_addr, area_id, mod_no, net_addr, innet_addr) < 0) {
        ret = -1;
        log_error(LOG_EMERG, "zigadd_dev");
    }
    /*
    sql_format(sql, 200, "update config set net_addr = %d, mod_no = %d, area_id = %d, innet_addr = %d where ext_addr = '%s'",
               net_addr, mod_no, area_id, innet_addr, ext_addr);
    if (sql_process(premap->sd_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }

    if (sql_process(premap->ram_db, sql, NULL, NULL) < 0) {
        log_error(LOG_EMERG, "sql_process");
        ret = -1;
    }
    */
    return ret;

}


int zigdev_tra(sqlite_cb cb, void * arg)
{
    char * sql = "select * from config;";
    ZIG_LOCK(premap);
    sql_process(premap->ram_db, sql, cb, arg);
    ZIG_UNLOCK(premap);
    return 0;
}










