#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "cache.h"
#include "list.h"
#include "modules.h"
#include "log.h"

#define  PTHREAD_SAFE_

#ifdef PTHREAD_SAFE_
#define mem_lock(x)     pthread_mutex_lock(&(x)->lock)
#define mem_unlock(x)   pthread_mutex_unlock(&(x)->lock)
#else
#define mem_lock(x)
#define mem_unlock(x)
#endif


#define     MAX_EXT             200

#define     MAX_USER_LEN        12

#define     M_CTRL            1
#define     M_QUERY             0


typedef struct module_info {
    char * table_name;
    struct list_head list;
    pthread_mutex_t lock;
    time_t last_time;           //�ϴδ������ݵ�ʱ��
    int mod_type;               //ģ������
    int ext_cnt;                //��ַ����
} module_info_t;


typedef struct ext_addr {
    struct list_head entry;         //���
    struct list_head ext_list;      //��ַ����ͷ
    uint64_t ext_addr;      //�����ַ
} ext_addr_t;

typedef struct mod_ctrl{                //����ģ��Ļ�����Ϣ
    struct list_head    entry;
    uint64_t            ext_addr;
    time_t              timestamp;          //���β���ʱ��
    uint16_t            opt_code;           //������
    char                user[MAX_USER_LEN+1];               //�����û�
} mod_ctrl_t;


typedef struct mod_query {              //��ѯģ��Ļ�����Ϣ
    struct list_head    entry;
    uint64_t            ext_addr;
    time_t              timestamp;          //����ʱ��
    struct data{
        uint8_t * pdata;
        int     size;
    } * pdata;
    int cnt;
} mod_query_t;


typedef struct mod_mix {
    struct list_head    entry;
    uint64_t            ext_addr;
    time_t              timestamp;
} mod_mix_t;

typedef struct mem_cache {
    module_info_t mod[MAX_MODULE_NO];
} mem_cache_t;


static mem_cache_t * pcache = NULL;

int mem_cache_init(void)
{
    int i;



    if (pcache != NULL)
        return 0;

    pcache = malloc(sizeof(*pcache));
    if (pcache == NULL)
        return -1;

    memset(pcache, 0, sizeof(*pcache));

    for (i = 0; i < MAX_MODULE_NO; i++) {
        INIT_LIST_HEAD(&pcache->mod[i].list);
        pthread_mutex_init(&pcache->mod[i].lock, NULL);
        pcache->mod[i].mod_type = g_zig_mod[i].mod_type;
    }

    return 0;
}

void mem_cache_destroy(void)
{
    free(pcache);
    return 0;
}

/*           ��ѯ��ַ�Ƿ����  ���ڷ���  �����ڷ���0NULL           */
ext_addr_t * mod_ext_exist(module_info_t * pinfo, uint64_t ext_addr)
{
    ext_addr_t * pext = NULL;

    list_for_each_entry(pext, &pinfo->list, entry) {
        if (pext->ext_addr == ext_addr)
            return pext;
    }
    return NULL;
}

/*         ��ӽڵ�                                                 */
static int mod_ext_add(module_info_t * pinfo, uint64_t ext_addr)
{
    ext_addr_t * pext = NULL;

    if (mod_ext_exist(pinfo, ext_addr))
        return 0;

    if (pinfo->ext_cnt > MAX_EXT) {
        log_error(LOG_NOTICE, "too much addr");
        return -1;
    }


    pext = malloc(sizeof(ext_addr_t));
    if (pext == NULL)
        return -1;
    INIT_LIST_HEAD(&pext->ext_list);
    pext->ext_addr = ext_addr;
    list_add(&pext->entry, &pinfo->list);
    pinfo->ext_cnt++;
    return 0;
}

static void mod_ext_del(module_info_t * pinfo, uint64_t ext_addr)
{
    ext_addr_t * pext = mod_ext_exist(pinfo, ext_addr);

    if (pext == NULL)
        return;

    list_del(&pext->entry);
    free(pext);
    return;
}
