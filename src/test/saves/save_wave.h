#include <stdint.h>


#define WAVE_DEBUG

#ifdef WAVE_DEBUG
#define debug(format,args...) printf(format, ##args)
#else
#define debug(format,args...)
#endif

#define     MAX_DATA_ITEM   (10)
#define     RTC_LEN         15

typedef enum {
    OPEN_SHIFT = 0,     //分闸位移
    OPEN_SPEED,     //分闸速度
    OPEN_ACCE,      //分闸加速度
    OPEN_RESV,
    CLOSE_SHIFT,
    CLOSE_SPEED,
    CLOSE_ACCE,
    CLOSE_RESV,
    WAVE_END
} wave_form_t;


/*        初始化          */
int wave_init(void);

/*        销毁              */
void wave_destroy(void);

/*        存储          */
int wave_add(wave_form_t wave_type, uint8_t * pdata, int size);


/*        搜索所有时间          */
char * wave_time(wave_form_t wave_type, int * cnt);

/*        搜索波形            */
int wave_search(wave_form_t wave_type, char * rtc_string, uint8_t * pdata, int * size);


/*      index 为索引   wave_time 为 wave_time的返回值    max为最大值    wave_time中的cnt                    */
char * get_wtime(char * wtime, int index, int max);
