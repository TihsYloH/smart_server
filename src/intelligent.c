#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "tcp_server.h"

#include "net_config.h"
#include "iic.h"
#include "log.h"
#include "buffer.h"
#include "zigbee_dev.h"
#include "events.h"
#include "send.h"

//#include "segvCatch.h"

int def_test(void)
{



    return 0;
}



int zigdev_test(void)
{
    char sql[200];

    sql_format(sql, 200, "insert into user values('test user', '123456', 'broofleg@163.com', '15895830938');");


    if (cache_opt_item(TBL_USER, sql) < 0)
        printf("\ncache_opt_item\n");

    sql_format(sql, 200, "insert into user values('TihsYloH', '123456', 'broofleg@163.com', '15895830938');");
    if (cache_opt_item(TBL_USER, sql) < 0)
        printf("\ncache_opt_item\n");

    sql_format(sql, 200, "select * from user where user_name = 'TihsYloH';");
    if (cache_search(TBL_USER, sql, NULL, NULL, 0) == 0) {
        printf("\nTihsYloH user find \n");
    } else {
        printf("\nTihsYloH user not find\n");
    }

    sql_format(sql, 200, "select * from user where user_name = 'TihsYloH__';");
    if (cache_search(TBL_USER, sql, NULL, NULL, 0) == 0) {
        printf("\nTihsYloH___ user find \n");
    } else {
        printf("\n\nTihsYloH___ user not find\n");
    }



    return 0;
}


void sig_handler(int signo)
{
    switch(signo) {
    case SIGINT:

        log_error(LOG_NOTICE, "server end due to int");

        break;
    default:
        log_error(LOG_NOTICE, "\na unknown signal\n");
        break;
    }
    return;
}

void signal_propose(void)
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);

    //sa.sa_handler = sig_handler;
    //sigaction(SIGINT, &sa, NULL);
    return;
}



//static int pid = 0;

void sys_abort()
{
    raise(SIGINT);
}

int main(int argc, char **argv)
{
    struct tm tm;

   // pid = getpid();


    init_abt_log(NULL, LOG_NOTICE);




    if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD) != SQLITE_OK) {       //多线程模式防止并发锁
        log_error(LOG_ERROR, "mutile thread error");
    }

    if (init_ds3231() < 0) {
        log_error(LOG_EMERG, "init_ds3231");
        return -1;
    }
    getTime(&tm);

    if (cache_init() < 0) {

        log_error(LOG_EMERG, "cache_init");
        return -1;
    }

    if (init_at24c02b() < 0) {
        log_error(LOG_NOTICE, "init_at24c02b");
    }
    g_dev_version = zigdev_version();

    if (zigdev_init() < 0) {
        log_error(LOG_NOTICE, "zigdev_init");
        return -1;
    }

    signal_propose();
    zigdev_test();
    //zigadd_dev(0x256833, 1, 1, 0, 1);
    tcp_server(NULL);


    cache_destroy();

    close_abt_log();
    return 0;
}
