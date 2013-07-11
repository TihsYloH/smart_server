
#include <stdint.h>

#include "zigbee_dev.h"
#include "sqlite_db.h"
#include "log.h"
#include "rbtree.h"
#include "list.h"

typedef int (*buffer_node_cb)(void * arg);

typedef enum {
    SEARCH_CACHE,
    SEARCH_SD
} SEARCH_T;

typedef struct cache_db_s {
    pthread_mutex_t lock;
    sqlite_db_t * db;       //sd卡上的数据库
    int tbl_items[TBL_NULL];

    int item_cnt;
} cache_db_t;



int          smtdb_table(sqlite_db_t *, char *, sqlite_cb cb, void * arg);
int          smtdb_search(sqlite_db_t *, char * sql, sqlite_cb cb, void * arg);
int          smtdb_opt(sqlite_db_t *, char * sql);


/*             初始化缓存数据库                     */
int cache_init(void);
/*             销毁缓存操作                                  */
void cache_destroy(void);

/*         执行sql语句             */
int cache_table(char * sql, sqlite_cb cb, void * arg);
/*            查找                              */
int cache_search(TABLE_T table, char * sql, sqlite_cb cb, void * arg, SEARCH_T type);

int cache_opt_item(TABLE_T table, char * sql);       //插入 删除等操作

/*        直接取出句柄 没release之前不能调用以上函数              */
sqlite3 * cache_get_db(void);

void cache_release_db(void);


