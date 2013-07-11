#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "save_wave.h"



#define     WAVE_FILE       "/mnt/wave_data"
#define     MAX_LENGTH      (1024*1024*256)



static char * wave_table[] = {"/mnt/breaker_wave/open_shift", "/mnt/breaker_wave/open_speed",
                       "/mnt/breaker_wave/open_acce", "/mnt/breaker_wave/open_resv",
                       "/mnt/breaker_wave/close_shift", "/mnt/breaker_wave/close_speed",
                       "/mnt/breaker_wave/close_acce", "/mnt/breaker_wave/close_resv", "\0"};



#pragma pack(1)             //字节对齐为1字节
typedef struct wave_index {
    int32_t version_start;
    int wave_cnt;           //波形数量
    struct item {
        char rtc_string[RTC_LEN];
    } item[MAX_DATA_ITEM];
    int32_t version_end;
} wave_index_t;
#pragma pack()

typedef struct wave {
    wave_index_t * wave_index[WAVE_END];
} wave_t;

static wave_t * pwave = NULL;


static int file_write(void * pdata, int size, FILE * fp);
static int file_read(void * pdata, int size, FILE * fp);

static int32_t file_version;

int wave_init(void)
{
    int i, fd;
    FILE * fp;

    void * dptr;

    char cmd[100];

    file_version = 255;

    if (pwave != NULL)
        return 0;

    pwave = malloc(sizeof(wave_t));

    if (pwave == NULL) {
        printf("\nwave_init\n");
        return -1;
    }
    system("mkdir \"/mnt/breaker_wave\"");
    for (i = 0; i < WAVE_END; i++) {
        snprintf(cmd, 100, "mkdir %s", wave_table[i]);
        system(cmd);

        snprintf(cmd, 100, "%s/index", wave_table[i]);
        fp = fopen(cmd, "r+");
        if (fp == NULL) {
            wave_index_t wave;

            fp = fopen(cmd, "w+");
            if (fp == NULL)
                return -1;
            memset(&wave, 0, sizeof(wave));
            if (file_write(&wave, sizeof(wave), fp) < 0) {
                printf("\nfile_write\n");
                fclose(fp);
                return -1;
            }
        }
        fclose(fp);

        while ((fd = open(cmd, O_RDWR)) < 0) {
            if (errno == EINTR)
                continue;
            perror("open");
            return -1;
        }
        dptr = mmap(NULL, sizeof(wave_index_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        if (dptr == MAP_FAILED) {
            perror("\nmmap\n");
            printf("\nmmap failed... file = %s, line = %d\n", __FILE__, __LINE__);
            exit(1);
        }

        pwave->wave_index[i] = dptr;

        if (pwave->wave_index[i]->version_end != pwave->wave_index[i]->version_start) {
            printf("\nfile not match\n");
            memset(dptr, 0, sizeof(wave_index_t));
        }
        close(fd);          //关闭句柄

    }

    return 0;
}

void wave_destroy(void)
{
    if (pwave) {
        int i;
        for (i = 0; i < WAVE_END; i++) {
            munmap(pwave->wave_index[i], sizeof(wave_index_t));
        }
        free(pwave);
    }
    pwave = NULL;
    return;
}

static int file_write(void * pdata, int size, FILE * fp)
{
    int write_cnt = 0;

    while (1) {
        write_cnt = fwrite(pdata, 1, size, fp);
        if (write_cnt != size) {

            debug("\nsize not match\n");
            if (ferror(fp)) {
               size -= write_cnt;
               clearerr(fp);
               continue;
            }

            if (feof(fp)) {
                printf("\nfwrite error\n");
                return -1;
            }

        } else
            break;
    }
    return 0;
}

static int file_read(void * pdata, int size, FILE * fp)
{
    int read_cnt = 0;
    int this_read;

    while (read_cnt < size) {
        this_read = fread(pdata, 1, 1000, fp);
        read_cnt +=  this_read;
        if (this_read < 1000) {
            if (ferror(fp)) {

               clearerr(fp);
               continue;
            }
            if (feof(fp)) {
                break;
            }
        }
    }

    return read_cnt;
}


int wave_add(wave_form_t wave_type, uint8_t * pdata, int size)
{
    time_t tm = time(NULL);
    char rtc_string[RTC_LEN];
    char file_path[100];

    int wave_cnt;
    FILE * fp;
    struct tm *t;


    wave_index_t * pindex = pwave->wave_index[wave_type];

    t = localtime(&tm);

    t->tm_year += 1900;
    t->tm_mon += 1;
    t->tm_mday = t->tm_mday;



    snprintf(rtc_string, RTC_LEN, "%04d%02d%02d%02d%02d%02d",
             t->tm_year, t->tm_mon, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    debug("\nrtc_string = %s\n", rtc_string);
    snprintf(file_path, 100, "%s/%s", wave_table[wave_type], rtc_string);

    debug("\nfile_path = %s\n", file_path);
    file_version++;


    wave_cnt = pindex->wave_cnt;

    fp = fopen(file_path, "w+");
    if (fp == NULL) {
        printf("\nfopen %s %d\n", __FILE__, __LINE__);
        return -1;
    }
    if (file_write(pdata, size, fp) < 0) {
        unlink(file_path);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    pindex->version_start = file_version;
    if (wave_cnt < MAX_DATA_ITEM) {
        memcpy(pindex->item[wave_cnt].rtc_string, rtc_string, RTC_LEN);
        pindex->wave_cnt++;
    } else {
        debug("\nwave_cnt = %d\n", wave_cnt);
        memmove(pindex->item, pindex->item+1, (MAX_DATA_ITEM - 1)*RTC_LEN);

        snprintf(file_path, 100, "%s/%s", wave_table[wave_type], pindex->item[0].rtc_string);
        unlink(file_path);

        memcpy(pindex->item[MAX_DATA_ITEM-1].rtc_string, rtc_string, RTC_LEN);


    }
    pindex->version_end = file_version;

    msync(pindex, sizeof(*pindex), MS_SYNC);
    return 0;
}

char * get_wtime(char * wtime, int index, int max)
{
    if (index >= max)
        return NULL;

    return wtime + index * RTC_LEN;
}

char * wave_time(wave_form_t wave_type, int * cnt)
{
    static char rtc_string[MAX_DATA_ITEM * RTC_LEN];

    wave_index_t * pindex = pwave->wave_index[wave_type];


    memcpy(rtc_string, pindex->item, RTC_LEN * MAX_DATA_ITEM);

    * cnt = pindex->wave_cnt;
    return rtc_string;
}

int wave_search(wave_form_t wave_type, char * rtc_string, uint8_t * pdata, int * size)
{
    FILE * fp;
    char path[100];

    snprintf(path, 100, "%s/%s", wave_table[wave_type], rtc_string);


    fp = fopen(path, "r+");
    if (fp == NULL) {
        printf("\nfopen %s %d\n", __FILE__, __LINE__);
        return -1;
    }

    *size = file_read(pdata, *size, fp);
    fclose(fp);
    return 0;
}
