#include <pthread.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "log.h"

#include "sql_db.h"

#define THREAD_SAFE_



#ifdef THREAD_SAFE_
pthread_mutex_lock lock = PTHREAD_MUTEX_INITIALIZER;
#define SQL_LOCK()  pthread_mutex_lock(&lock);
#define SQL_UNLOCK() pthread_mutex_unlock(&lock);
#else
#define SQL_LOCK()
#define SQL_UNLOCK()

#endif



int sqlite_db_open(sqlite_db_t * sql)
{
    int exist = 0;
    int fd;

    while ((fd = open(SQL_DB, RDONLY | O_CREAT | O_EXCL)) < 0) {
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
        unlink(SQL_DB);
        close(fd);
    }
    if (sqlite3_open(SQL_DB, &sql->sqlite_db) != SQLITE_OK) {    //数据库打开错误
        log_error(LOG_ERROR, "sqlite3_open");
        return -1;
    }
    if (exist)
        return 0;
    result = sqlite3_exec(sql->sqlite_db, "create table USER(user_name nvarchar(32) primary key, user_code nvarchar(32))",
                          NULL, NULL, errmsg );     //用户表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table AREA( area_id integer primary key autoincrement, area_name nvarchar(32), code nvarchar(32))",
                          NULL, NULL, errmsg ); //房间表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table OWEN( owener_id integer primary key autoincrement, user_name nvarchar(32), area_id integer)",
                          NULL, NULL, errmsg ); //用户拥有房间表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table TEM( tem_id integer primary key autoincrement, area_id integer, timestamp nvarchar(32), tem_data float, tem_gate float)",
                          NULL, NULL, errmsg ); //温度数据表
    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table APP( app_id integer primary key autoincrement, area_id integer, app_name nvarchar(32), red_id integer)",
                          NULL, NULL, errmsg ); //家电


    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }

    result = sqlite3_exec(sql->sqlite_db, "create table RED( red_id integer primary key autoincrement, data_path nvarchar(32))",
                          NULL, NULL, errmsg ); //红外编码表

    if(result != SQLITE_OK ) {
        log_error(LOG_ERROR, "sqlite3_exec");
        return -1;
    }
    return 0;
}

int sqlite_db_close(sqlite_db_t * sql)
{
    sqlite3_close(sql->sqlite_db);
    return 0;
}
