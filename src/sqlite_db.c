#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "log.h"
#include "sqlite_db.h"

#define THREAD_SAFE_



#ifdef THREAD_SAFE_
pthread_mutex_t  lock = PTHREAD_MUTEX_INITIALIZER;
#define SQL_LOCK()  pthread_mutex_lock(&lock);
#define SQL_UNLOCK() pthread_mutex_unlock(&lock);
#else
#define SQL_LOCK()
#define SQL_UNLOCK()
#endif



#define SQLS_THREAD_SAFE
#define MAX_SQL_BUFFER_CNT     500

#define SQL_INSERT  "INSERT INTO %s VALUES (%s);"


typedef struct sql_s {
    smpslab_t * slab;
#ifdef SQLS_THREAD_SAFE
    pthread_mutex_t lock;
#endif
} sql_t;


#ifdef SQLS_THREAD_SAFE
#define     SQLS_LOCK()  pthread_mutex_lock(&sqls->lock)
#define     SQLS_UNLOCK() pthread_mutex_unlock(&sqls->lock)
#else
#define    SQLS_LOCK()
#define    SQLS_UNLOCK()
#endif

#define  MAX_SQL_NUM     20
#define  MAX_SQL_SIZ     768


const char *g_sql_table[] = {
    "USER",
    "AREA",
    "OWEN",
    "TEM",
    "APP",
    "RED_CODE",
    "IR_ZIGBEE"
    "RED_KEY",
    "IR_KEY"
    "CTL_BLIND",
    "MODULE",
    "BLOOD_PRESS"
};


static sql_t * sqls;

int sqlslab_init(void)
{
    sqls->slab = smpslab_init(MAX_SQL_NUM, MAX_SQL_SIZ);

    if (sqls->slab == NULL) {
        log_error(LOG_DEBUG, "sql_init");
        free(sqls);
        return -1;
    }

#ifdef SQLS_THREAD_SAFE
    pthread_mutex_init(&sqls->lock, NULL);
#endif

    return 0;
}


void sqlslab_destroy(void)
{
    smpslab_destroy(sqls->slab);
    free(sqls);
}


/*                 格式化语句 到pbuf中  长度最大为n                     */
char * sql_format(char *pbuf, int n, char * format, ...)
{
    int cnt;
    va_list arglst;


    va_start(arglst, format);
    cnt = vsnprintf(pbuf, n, format, arglst);
    va_end(arglst);

    if (cnt < 0) {
        log_error(LOG_DEBUG, "sql_create");
        return NULL;
    }
    return pbuf;
}



/*            生成一条sql语句              */
char * sql_create(char * format, ...)
{
    int cnt;
    va_list arglst;
    char * pbuf;
    SQLS_LOCK();
    pbuf = smpslab_malloc(sqls->slab);
    SQLS_UNLOCK();

    if (pbuf == NULL) {
        log_error(LOG_DEBUG, "sql_create");
        return NULL;
    }

    va_start(arglst, format);
    cnt = vsnprintf(pbuf, MAX_SQL_SIZ, format, arglst);
    va_end(arglst);

    if (cnt < 0) {
        log_error(LOG_DEBUG, "sql_create");
        return NULL;
    }
    return pbuf;
}


/*             删除一条sql语句                         */
void sql_destroy(char * sql)
{
    SQLS_LOCK();
    smpslab_free(sqls->slab, sql);
    SQLS_UNLOCK();
}



static int count_cb(void * db, int n_column, char ** column_value, char ** column_name)
{
    int cnt =  0;


    cnt = atoi(column_value[0]);

    *((int *)db) = cnt;
    printf("\n cnt = %d", cnt);
    return 0;

}


int sql_tblcnt(sqlite_db_t * sql_db, char * table)
{

    int ret = 0;
    char sql[200];

    sql_format(sql, 200, "select count(*) from %s;", table);
    if (sql_process(sql_db, sql, count_cb, &ret) < 0)
        log_error(LOG_NOTICE, "sql_tblcnt");

    //printf("\nret = %d\n", ret);
    return ret;
}


int sql_process(sqlite_db_t * sql_db, char *sql, sqlite_cb cb, void *arg)
{
    char * zErrMsg;
    int result;

    result = sqlite3_exec(sql_db->sqlite_db , sql , cb , arg, &zErrMsg );

    if (result != SQLITE_OK && result != SQLITE_CONSTRAINT) {
        printf("\nresult = %d\n", result);
        log_error(LOG_NOTICE, zErrMsg);
        return -1;
    }

    return 0;
}


int sqlite_db_open(sqlite_db_t * sql, char * sql_db_patch)
{
    int exist = 0;
    int fd;
    int result;
    char * errmsg;



    while ((fd = open(sql_db_patch, O_RDWR | O_CREAT | O_EXCL)) < 0) {
        if (errno == EINTR)
            continue;
        if (errno == EEXIST) {
            exist = 1;  //文件已经存在
            break;
        }
        log_error(LOG_ERROR, "open");
        return -1;
    }

    if (fd > 0) {
        exist = 0;  //文件不存在
        unlink(sql_db_patch);
        close(fd);
    }



    if (sqlite3_open(sql_db_patch, &sql->sqlite_db) != SQLITE_OK) {    //数据库打开错误
        log_error(LOG_ERROR, "sqlite3_open");
        unlink(sql_db_patch);
        if (sqlite3_open(sql_db_patch, &sql->sqlite_db) != SQLITE_OK) {
            log_error(LOG_ERROR, "sqlite3 fatal error");
            return -1;
        }
        exist = 0;
    }

    if (exist)
        return 0;

    result = sqlite3_exec(sql->sqlite_db, "create table USER(user_name nvarchar(32) primary key,\
                          user_password nvarchar(32) NOT NULL, user_email nvarchar(32), \
                          user_phone nvarchar(20));",
                          NULL, NULL, &errmsg );     //用户表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table AREA( area_id integer primary key,\
                          area_name nvarchar(32) NOT NULL);",
                          NULL, NULL, &errmsg ); //区域表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);

        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table OWEN( owener_id integer primary key autoincrement,\
                          user_name nvarchar(32) not NULL, area_id integer not NULL);",
                          NULL, NULL, &errmsg ); //用户拥有房间表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table TEM( tem_id integer primary key autoincrement, area_id integer,\
                          timestamp nvarchar(32), tem_data float, tem_gate float, ext_addr nvarchar(8));",
                          NULL, NULL, &errmsg ); //温度数据表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table APP( app_id integer primary key autoincrement, \
                          area_id integer NOT NULL, app_name nvarchar(32) NOT NULL,\
                          red_id integer);",
                          NULL, NULL, &errmsg ); //家电


    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table IR_CODE( ir_code_id integer primary key,\
                          ir_name integer not null);",
                          NULL, NULL, &errmsg ); //红外编码表

    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table IR_ZIGBEE( ir_zig_id integer primary key autoincrement,\
                          red_code_id integer,\
                          ext_addr nvarchar[30] not NULL,\
                          version  integer not NULL);",
                          NULL, NULL, &errmsg);
    if (result != SQLITE_OK) {              //红外和 zigbee的映射表
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d error msg = %s\n", __LINE__,errmsg );
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table RED_KEY( red_key_id integer primary key,\
                          red_code_id integer NOT NULL, red_path nvarchar[30] not NULL);",
                          NULL, NULL, &errmsg ); //红外键码表

    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    /*        创建一个红外编码表存储键值                                           */
    result = sqlite3_exec(sql->sqlite_db, "create table IR_KEY(\
                          ir_key_type integer not null, \
                          ir_key_word integer not null,\
                          ir_key_code integer NOT NULL, \
                          ir_key_data BLOB not NULL,\
                          ir_key_version integer not null,\
                          primary key(ir_key_type, ir_key_word, ir_key_code));",
                          NULL, NULL, &errmsg ); //红外键码表
    if (result != SQLITE_OK) {
        log_error(LOG_ERROR, errmsg);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table CTRL_BLIND( ctrl_blind_id integer primary key autoincrement,\
                          area_id integer NOT NULL, user_name nvarchar[32] not NULL,\
                          action_time nvarchar[50] NOT NULL, action_direction integer NOT NULL,\
                          ext_addr nvarchar[30] NOT NULL\
                          );",
                          NULL, NULL, &errmsg ); //窗帘表

    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table MODULE( ext_addr nvarchar[20] primary key,\
                          moudle_name integer NOT NULL, area_id integer NOT NULL);",
                          NULL, NULL, &errmsg ); //模块表

    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, errmsg);
        printf("\nsqlite3_exec line = %d\n", __LINE__);
        goto err_quit;
    }

    /*     血压表             */
    result = sqlite3_exec(sql->sqlite_db, "create table BLOOD_PRESS( time integer primary key,\
                          ext_addr nvarchar[20] not null,\
                          press_high float NOT NULL, press_low float NOT NULL, \
                          rate float NOT NULL, tem float NOT NULL);",
                          NULL, NULL, &errmsg );
    return 0;

err_quit:
    unlink(sql_db_patch);
    return -1;
}

int sqlite_db_close(sqlite_db_t * sql)
{
    sqlite3_close(sql->sqlite_db);
    return 0;
}





smpslab_t * smpslab_init(int unit_cnt, int unit_zie)
{
    smpslab_t * slab = malloc(sizeof(smpslab_t));
    int i;
    uint8_t * dptr;
    int size;

    if (slab == NULL) {
        log_error(LOG_EMERG, "smpslab_init malloc");
        goto exit0;
    }

    size = unit_zie * sizeof(smp_unit_t);
    slab->total_size = unit_cnt * (size);
    slab->area_siz = unit_zie;

    INIT_LIST_HEAD(&slab->ufree);

    slab->dptr = malloc(slab->total_size);

    if (slab->dptr == NULL) {
       log_error(LOG_EMERG, "smpslab_init malloc");
       goto exit1;
    }

    dptr = slab->dptr;
    for (i = 0; i < unit_cnt; i++) {
        list_add((struct list_head *)dptr, &slab->ufree);
        dptr += size;
    }

    return slab;
exit1:
    free(slab);
exit0:
    return NULL;

}

void smpslab_destroy(smpslab_t * slab)
{
    free(slab->dptr);
    free(slab);
}

void * smpslab_malloc(smpslab_t * slab)
{
    struct list_head * p;

    if (list_empty(&slab->ufree)) {
        log_error(LOG_DEBUG, "smpslab_malloc");
        return NULL;
    }

    p = slab->ufree.next;
    list_del(p);

    return (list_entry(p, smp_unit_t, entry))->pdata;
}

void  smpslab_free(smpslab_t * slab, void * dptr)
{
    smp_unit_t * punit;
    punit = (smp_unit_t*)(((uint8_t *)dptr) - sizeof(struct list_head));
    list_add(&punit->entry, &slab->ufree);
}
