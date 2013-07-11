/*
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include "rbtree.h"

#include "buffer.h"

#define     MAX_MODULES     100

#define     MAX_NODE        300
#define     PER_UNIT_CNT    100

#define BUF_LOCK(pnode) pthread_mutex_lock(&pnode->lock);
#define BUF_UNLOCK(pnode) pthread_mutex_unlock(&pnode->lock);


typedef int (*buffer_node_cb)(void * arg);

typedef int (*unit_handler)(void * arg, int size);

typedef struct buffer_unit_s {
    long timestamp;         //时间
    int cnt;                //数据长度
    uint16_t opt_code;
    uint8_t pdata[0];        //数据域
} buffer_unit_t;

typedef struct cache_buffer_s {
    struct rb_root root;
    int node_cnt;
    int max_node;
    uint32_t size;
    sqlite_db_t * sqlite_db;
}cache_buffer_t;

typedef struct buffer_node_s {
    struct rb_node node;
    uint64_t ext_addr;
    pthread_mutex_t lock;
    uint32_t module_no;     //模块编号      每个模块存储方式固定

    buffer_unit_t * uints[PER_UNIT_CNT];
    int ucnt;

} buffer_node_t;


static cache_buffer_t * pcache = NULL;

static int module00_cb(void * arg)
{
    return 0;
}

buffer_node_cb * modulecb_table[MAX_MODULES] = {module00_cb};

static inline int cmp_keyvalue(uint64_t data, uint64_t keyvalue)
{
    return (data > keyvalue);
}


static int cache_insert(cache_buffer_t * cache, buffer_node_t * data)
{
    struct rb_root *root = &cache->root;

    struct rb_node **new = &(root->rb_node), *parent = NULL;

    uint64_t value = data->ext_addr;

    while (*new) {
        buffer_node_t *this = container_of(*new, buffer_node_t, node);
        int result = cmp_keyvalue(value, this->ext_addr);

        parent = *new;
        if (result <= 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return -1;
    }

    data->ext_addr = value;


    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 0;
}



static buffer_node_t * cache_search(cache_buffer_t * cache, uint64_t value)
{
    struct rb_root *root = &cache->root;
    struct rb_node *node = root->rb_node;

    while (node) {
        buffer_node_t *data = container_of(node, buffer_node_t, node);
        int result;

        result = cmp_keyvalue(value, data->ext_addr);

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return data;
    }
    return NULL;
}



static void * cache_delete(cache_buffer_t * cache, uint64_t value)
{
    struct rb_root *root = &cache->root;
    buffer_node_t *data = cache_search(cache, value);

    if (data) {
        rb_erase(&data->node, root);
        return data;
    }
    return NULL;
}

static int cache_traverse (cache_buffer_t * cache)
{
    struct rb_root* root = &cache->root;
    struct rb_node* node;
    buffer_node_t* item;

    int i;

    for (node = rb_first (root); node; node = rb_next (node))
    {
        item = rb_entry (node, buffer_node_t, node);

        for (i = 0; i < item->ucnt; i++) {
            (modulecb_table[item->module_no])(item);
        }
        item->ucnt = 0;
    }
    return 0;

}

static int get_count_cb(void * arg, int n_column, char ** column_value, char ** column_name)
{
    cache_buffer_t * pcache;
    pcache = arg;

    pcache = pcache;

    return 0;
}

static int cache_add_cb(void * arg, int n_column, char ** column_value, char ** column_name)
{
    cache_buffer_t * pcache;
    pcache = arg;

    pcache = pcache;

    return 0;
}

int cache_add_unit(uint64_t ext_addr, void * pdata, int size)
{

    buffer_unit_t * punit;

    int i;

    if (pnode == NULL)
        return -1;

    BUF_LOCK(pnode);

    if (pnode->ucnt == PER_UNIT_CNT) {
        for (i = 0; i < item->ucnt; i++)
            (modulecb_table[item->module_no])(item);
        item->ucnt = 0;
    }

    punit = pnode->uints[pnode->ucnt] = malloc(size + sizeof(buffer_unit_t));
    if (punit == NULL) {
        log_error(LOG_DEBUG, "malloc");
        BUF_UNLOCK(pnode);
        return -1;
    }

    pnode->ucnt++;
    punit->timestamp = time(NULL);
    punit->cnt = size;
    punit->opt_code = 0;
    memcpy(punit->pdata, pdata, size);


    BUF_UNLOCK(pnode);
    return 0;
}

static int binary_search(buffer_unit_t * punit, long high, long dstValue, int type)
{
    long low = 0;
    high --;

    while (high - low > 1) {
        if (dstValue < punit[(high - low)/2].timestamp) {
            high = (high+low)/2;
        }
        else if (dstValue > punit[(high - low)/2].timestamp) {
            low = (high+low)/2;
        }
        else
            return (high + low)/2;
    }

    if (type == 0) {
        if (dstValue > punit[low].timestamp)
            return high;
        return low;
    } else if (type > 0) {
        if (dstValue < punit[high].timestamp)
            return low;
        return high;
    }
    return 0;
}

static inline void index_search(buffer_unit_t * punit, long high,
                            long min, long max,
                            long * pos1, long * pos2)
{
    *pos1 = binary_search(punit, high, min, 0);
    *pos2 = binary_search(punit, high, max, 1);
    return;
}

void * cache_search_unit(uint64_t ext_addr, long minstamp, long maxstamp, int * psiz,
                         unit_handler  handler)
{
    buffer_node_t * pnode = cache_search(pcache, ext_addr);

    long start, end;

    if (pnode == NULL)
        return NULL;

    BUF_LOCK(pnode);
    if (maxstamp == 0) {
        *psiz = pnode->ucnt;
        pnode->uints[pnode->ucnt];
        return pnode->uints[pnode->ucnt];
    }
    index_search(pnode->uints, minstamp, maxstamp, &start, &end);

    while (start <= end) {
        handler(pnode->uints[start]->pdata, cnt);
    }
    BUF_UNLOCK(pnode);
    return pnode->uints[pnode->ucnt];

}

int cache_add_node(uint64_t ext_addr, uint32_t module_no)
{
    buffer_node_t * pnode = malloc(sizeof(buffer_node_t));

    if (pnode == NULL) {
        log_debug(LOG_DEBUG, "malloc");
        return -1;
    }

    memset(pnode, 0, sizeof(*pnode));

    pthread_mutex_init(&pnode->lock, NULL);
    pnode->ext_addr = ext_addr;
    pnode->module_no = module_no;
    pcache->node_cnt++;
    return 0;
}

int cache_del_node(uint64_t ext_addr)
{
    buffer_node_t * pnode;

    if ((pnode = cache_delete(pcache, ext_addr)) == NULL)
        return -1;

    free(pnode);
    pcache->node_cnt--;
    return 0;
}

int cache_init(void)
{
    char * sql;

    if (pcache != NULL)
        return 0;

    pcache = malloc(sizeof(*pcache));

    if (pcache == NULL) {
        log_debug(LOG_DEBUG, "malloc");
        goto exit0;
    }

    memset(pcache, 0, sizeof(*pcache));

    pcache->root = RB_ROOT;

    pcache->max_node = MAX_NODE;

    if (sqlite_db_open(pcache->sqlite_db) < 0) {
        log_debug(LOG_DEBUG, "sqlite_db_open");
        goto exit1;
    }

    sql = "select count(*) from module";
    if (sql_process(pcache->sqlite_db, sql, get_count_cb, pcache) < 0) {
        log_debug(LOG_DEBUG, "sql_process");
        goto exit2;
    }

    sql = "select * from module";
    if (sql_process(pcache->sqlite_db, sql, cache_add_cb, pcache) < 0) {
        log_debug(LOG_DEBUG, "sql_process");
        goto exit3;
    }


    return 0;

exit3:
exit2:
    sqlite_db_close(pcache->sqlite_db);
exit1:
    free(pcache);
exit0:
    return -1;

}


int cache_destroy(void)
{
    sqlite_db_close(pcache->sqlite_db);

    free(pcache);
}
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "buffer.h"
#include "events.h"

#define xDEBUG_CACHE

#ifdef debug
    #undef debug
#endif

#ifdef DEBUG_CACHE
#define debug(format,args...) printf(format, ##args)
#else
#define debug(format,args...)
#endif

#define     MAX_MODULES     100

#define     MAX_NODE        300
#define     PER_UNIT_CNT    100

#define     MAX_CACHE_SIZ   (1024*1024*1024)            //1M
#define     TMP_DB_PATH     "/tmp/tmp_db"



//#define CACHE_LOCK(pnode)
//#define CACHE_UNLOCK(pnode)

#define CACHE_LOCK(pnode) pthread_mutex_lock(&pnode->lock);
#define CACHE_UNLOCK(pnode) pthread_mutex_unlock(&pnode->lock);





static cache_db_t * pcache = NULL;

#ifdef DEBUG_CACHE
int debug_called(void * db, int n_column, char ** column_value, char ** column_name)
{
    int i;

    printf("\nn_column = %d\n", n_column);
    printf("\n");
    for (i = 0; i < n_column; i++) {
       printf("%s = %s   ", column_name[i], column_value[i]);
    }



    printf("\n");
    return 0;

}
#endif


int cache_init(void)
{
    //sqlite_db_t * db;


    if (pcache != NULL)
        return 0;

    pcache = malloc(sizeof(*pcache));
    if (pcache == NULL) {
        log_error(LOG_EMERG, "malloc");
        return -1;
    }

    memset(pcache, 0, sizeof(*pcache));

    pcache->db = malloc(sizeof(sqlite_db_t));


    if (pcache->db == NULL) {
        log_error(LOG_ERROR, "pcache");
        exit(1);
    }

    pthread_mutex_init(&pcache->lock, NULL);


    if (sqlite_db_open(pcache->db, SQL_DB) < 0) {

        free(pcache);
        log_error(LOG_NOTICE, "sqlite db open");
        return -1;
    }



    return 0;
}



void cache_destroy(void)
{
    pthread_mutex_destroy(&pcache->lock);
;
    sqlite_db_close(pcache->db);

    free(pcache->db);
    free(pcache);
    pcache = NULL;
    unlink(TMP_DB_PATH);
}




/*
static int getfile_size(char * path)
{
    int fd;
    int ret = -1;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return ret;
    ret = lseek(fd, 0, SEEK_END);

    if (ret >= MAX_CACHE_SIZ)
        ret = 0;
    close(fd);

    return ret;
}

*/

int          smtdb_opt(sqlite_db_t *pdb, char * sql)
{
    int ret = -1;

    //printf("\nxxx ret = %d\n", ret);


    do {

        if (sql_process(pdb, sql, NULL, NULL) < 0) {
            log_error(LOG_DEBUG, "sql_process");
            break;
        }

        pcache->item_cnt++;
        ret = 0;
    } while (0);
    //printf("\nxxx ret = %d\n", ret);


    return  ret;
}


int cache_opt_item(TABLE_T table, char * sql)       //插入 删除等操作
{
    int ret = -1;
    if (table >= TBL_NULL)
        return -1;
    //printf("\nxxx ret = %d\n", ret);
    CACHE_LOCK(pcache);

    do {

        if (sql_process(pcache->db, sql, NULL, NULL) < 0) {
            log_error(LOG_DEBUG, "sql_process");
            break;
        }

        pcache->item_cnt++;
        ret = 0;
    } while (0);
    //printf("\nxxx ret = %d\n", ret);
    CACHE_UNLOCK(pcache);

    return  ret;
}

sqlite3 * cache_get_db(void)
{
    CACHE_LOCK(pcache);
    return pcache->db->sqlite_db;
}

void cache_release_db(void)
{
    CACHE_UNLOCK(pcache);
}


int          smtdb_table(sqlite_db_t * pdb, char *sql, sqlite_cb cb, void * arg)
{
    if (sql_process(pdb, sql, cb, arg) < 0) {
        log_error(LOG_NOTICE, "cache_table");
    }
    return 0;
}

int cache_table(char * sql, sqlite_cb cb, void * arg)
{
    CACHE_LOCK(pcache);
    if (sql_process(pcache->db, sql, cb, arg) < 0) {
        log_error(LOG_NOTICE, "cache_table");
    }
    CACHE_UNLOCK(pcache);
    return 0;
}

int          smtdb_search(sqlite_db_t * pdb, char * sql, sqlite_cb cb, void * arg)
{
    int is_find = -1;
    char **azResult;
    char *zErrMsg;
    int nrow, ncolumn;

    int i;

    do {
        if (sqlite3_get_table( pdb->sqlite_db , sql , &azResult , &nrow , &ncolumn , &zErrMsg)) {
            log_error(LOG_DEBUG, zErrMsg);
            break;
        }

        nrow += 1;

        if (cb == NULL) {
            if (nrow > 1) {
                is_find = 0;
                break;
            }
            is_find = -1;
            break;
        }

        for( i = 1 ; i < nrow ; i++ ) {
            cb(arg, ncolumn, azResult, azResult + (nrow * ncolumn));
            is_find = 0;
        }
        //释放掉  azResult 的内存空间
        sqlite3_free_table( azResult );

    } while (0);

    CACHE_UNLOCK(pcache);

    return is_find;
}


int cache_search(TABLE_T table, char * sql, sqlite_cb cb, void * arg, SEARCH_T type)
{
    int is_find = -1;
    char **azResult;
    char *zErrMsg;
    int nrow, ncolumn;

    int i;
    if (table >= TBL_NULL )
        return is_find;

    CACHE_LOCK(pcache);

    do {
        if (sqlite3_get_table( pcache->db->sqlite_db , sql , &azResult , &nrow , &ncolumn , &zErrMsg)) {
            log_error(LOG_DEBUG, zErrMsg);
            break;
        }

        nrow += 1;

        if (cb == NULL) {
            if (nrow > 1) {
                is_find = 0;
                break;
            }
            is_find = -1;
            break;
        }

        for( i = 1 ; i < nrow ; i++ ) {
            cb(arg, ncolumn, azResult, azResult + (nrow * ncolumn));
            is_find = 0;
        }
        //释放掉  azResult 的内存空间
        sqlite3_free_table( azResult );

    } while (0);

    CACHE_UNLOCK(pcache);

    return is_find;
}





