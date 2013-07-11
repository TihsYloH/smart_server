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
    OPEN_SHIFT = 0,     //��բλ��
    OPEN_SPEED,     //��բ�ٶ�
    OPEN_ACCE,      //��բ���ٶ�
    OPEN_RESV,
    CLOSE_SHIFT,
    CLOSE_SPEED,
    CLOSE_ACCE,
    CLOSE_RESV,
    WAVE_END
} wave_form_t;


/*        ��ʼ��          */
int wave_init(void);

/*        ����              */
void wave_destroy(void);

/*        �洢          */
int wave_add(wave_form_t wave_type, uint8_t * pdata, int size);


/*        ��������ʱ��          */
char * wave_time(wave_form_t wave_type, int * cnt);

/*        ��������            */
int wave_search(wave_form_t wave_type, char * rtc_string, uint8_t * pdata, int * size);


/*      index Ϊ����   wave_time Ϊ wave_time�ķ���ֵ    maxΪ���ֵ    wave_time�е�cnt                    */
char * get_wtime(char * wtime, int index, int max);
