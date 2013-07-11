#ifndef SQLITE_DB_H
#define SQLITE_DB_H

#include <stdarg.h>
#include <stdint.h>

#include "list.h"
#include "sqlite3.h"

#define SQL_DB    "/mnt/sqlite.db"

/*************************************/
typedef enum {
       TBL_USER = 0,
       TBL_AREA,
       TBL_OWEN,
       TBL_TEM,
       TBL_APP,
       TBL_IR_CODE,
       TBL_IR_ZIGBEE,
       TBL_RED_KEY,
       TBL_IR_KEY,
       TBL_CTRL_BLIND,
       TBL_MODULE,
       TBL_BLOOD,
       TBL_NULL
} TABLE_T;

extern const char *g_sql_table[];

/***************************************/

typedef struct sqlite_db {
    sqlite3 * sqlite_db;
} sqlite_db_t;


typedef struct smpunit_s {
    struct list_head entry;
    uint8_t pdata[0];
} smp_unit_t;

typedef struct smpslab_s {
    struct list_head ufree;         //空闲列表
    uint8_t * dptr;                 //数据区指针
    int total_size;           //总共大小
    int area_siz;               //每个单元大小
} smpslab_t;


typedef int (*sqlite_cb)(void * arg, int n_column, char ** column_value, char ** column_name);



int sql_tblcnt(sqlite_db_t * sql, char * table);

/*                 格式化语句 到pbuf中  长度最大为n                     */
char * sql_format(char *pbuf, int n, char * format, ...);
/*            打开一个sqlite 句柄                      */
int sqlite_db_open(sqlite_db_t * sql, char * sql_db_path);

/*             关闭一个sqlite句柄                      */
int sqlite_db_close(sqlite_db_t * sql);



/*       处理sql语句  sqlite 回调   arg 为参数         */
int sql_process(sqlite_db_t * sql_db, char *sql, sqlite_cb cb, void *arg);

/*        分配一个数量为  unit_cnt   每个单元大小为    unit_size      */
smpslab_t * smpslab_init(int unit_cnt, int unit_size);

void smpslab_destroy(smpslab_t * slab);

/*                     分配存储空间                            */
void * smpslab_malloc(smpslab_t * slab);

void  smpslab_free(smpslab_t * slab, void * dptr);

#endif // SQLITE_DB_H
