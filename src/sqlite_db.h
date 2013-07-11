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
    struct list_head ufree;         //�����б�
    uint8_t * dptr;                 //������ָ��
    int total_size;           //�ܹ���С
    int area_siz;               //ÿ����Ԫ��С
} smpslab_t;


typedef int (*sqlite_cb)(void * arg, int n_column, char ** column_value, char ** column_name);



int sql_tblcnt(sqlite_db_t * sql, char * table);

/*                 ��ʽ����� ��pbuf��  �������Ϊn                     */
char * sql_format(char *pbuf, int n, char * format, ...);
/*            ��һ��sqlite ���                      */
int sqlite_db_open(sqlite_db_t * sql, char * sql_db_path);

/*             �ر�һ��sqlite���                      */
int sqlite_db_close(sqlite_db_t * sql);



/*       ����sql���  sqlite �ص�   arg Ϊ����         */
int sql_process(sqlite_db_t * sql_db, char *sql, sqlite_cb cb, void *arg);

/*        ����һ������Ϊ  unit_cnt   ÿ����Ԫ��СΪ    unit_size      */
smpslab_t * smpslab_init(int unit_cnt, int unit_size);

void smpslab_destroy(smpslab_t * slab);

/*                     ����洢�ռ�                            */
void * smpslab_malloc(smpslab_t * slab);

void  smpslab_free(smpslab_t * slab, void * dptr);

#endif // SQLITE_DB_H
