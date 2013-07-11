

#include <time.h>
#include <fcntl.h>
#include <linux/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "iic.h"
#include <linux/bcd.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>



#define     RT_DEVICE   "/dev/i2c0"
#define     EEPROM      "/dev/i2c1"


#define HR24 1
#define HR12 0
#define RTC_TIME_SCALE	0x10

#define SIGRTC 26
#define ALARM_IRQ 1
#define TICK_IRQ 2

/* Device DS3231 Slave address */
#define DS3231_ADR           0x68

/* DS3231 Time registers address*/
#define DS3231_REG_SEC        0x00    //秒寄存器地址
#define DS3231_REG_MIN        0x01    //分寄存器地址
#define DS3231_REG_HOUR       0x02    //小时寄存器地址
#define DS3231_REG_WEEK       0x03    //星期寄存器地址
#define DS3231_REG_DATE       0x04    //日期寄存器地址
#define DS3231_REG_MONTH      0x05    //月寄存器地址
#define DS3231_REG_YEAR       0x06    //年寄存器地址

/* DS3231 Special Funtion registers address*/
#define DS3231_REG_CONTROL    0x0e      //控制存器地址
#define DS3231_REG_STATUS     0x0f      //状态存器地址
#define DS3231_REG_COMPENSATE 0x10      //补偿存器地址
#define DS3231_REG_TEM_MSB    0x11      //温度寄存器地址(高字节)
#define DS3231_REG_TEM_LSB    0x12      //温度存器地址(低字节)



static int fd_i2c;
static int fd_i2c0;

int init_ds3231()
{
    unsigned char i2cdata[3];
    int ret;

    fd_i2c = open(RT_DEVICE, O_RDWR);
    if (fd_i2c == -1)
    {
        printf("Can't open %s!\n", RT_DEVICE);
        return -1;
    }
    else
    {
        ret = ioctl(fd_i2c, 0x0703, DS3231_ADR);	//首先设置DS3231的地址
        printf("Set ds3231 address OK! \n");

        i2cdata[0] = DS3231_REG_CONTROL;			//其次设置DS3231的控制寄存器
        i2cdata[1] = 0x04;               			//00000100:review ds3231.pdf for more detail
        if(write(fd_i2c, &i2cdata[0], 2) != 2)
            printf("init ds3231 error !\n");
        else
            printf("init ds3231 OK!\n");
        return 1;
    }
}

void getTime(struct tm *tm)
{
  int i;
  unsigned char buf[7];
  unsigned char i2cdata[3];
  char rtc_string[50];

  if(fd_i2c) {
    for(i=0;i<7;i++) {
     i2cdata[0] = i;
     if (write(fd_i2c, &i2cdata[0], 1) != 1)		//first:write time address
        printf("write error at %d\n", i);
     if (read(fd_i2c, &i2cdata[0], 1) != 1) 	//second:read data
        printf("read error at %d\n", i);
     else
        buf[i] = i2cdata[0];
    }
    tm->tm_sec = BCD2BIN(buf[0]);
    tm->tm_min = BCD2BIN(buf[1]);
    tm->tm_hour = BCD2BIN(buf[2]);
    tm->tm_wday = BCD2BIN(buf[3]);				//这是星期,实际应用中不用
    tm->tm_mday = BCD2BIN(buf[4]);

    buf[5] = buf[5]&&0x7f;   //don't care bit 7 in month register
    tm->tm_mon = BCD2BIN(buf[5]);
    tm->tm_year = BCD2BIN(buf[6])+2000;

    printf("\n-----read rtc %02d%02d%02d%02d%d.%02d-----\n",
           tm->tm_mon,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_year,tm->tm_sec);
    sprintf(rtc_string,"date %02d%02d%02d%02d%d.%02d",tm->tm_mon,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_year,tm->tm_sec);
    system(rtc_string);
  }
  return ;

}

int  set_time(unsigned int year0,unsigned int year1,unsigned int month,unsigned int day,unsigned int hour,unsigned int min,unsigned int sec)
{
    int i;
    unsigned char buf[7];
    unsigned char i2cdata[3];
    struct tm rtc_tm;

    rtc_tm.tm_year =year1;
    rtc_tm.tm_mon = month;
    rtc_tm.tm_mday = day;
    rtc_tm.tm_hour = hour;
    rtc_tm.tm_min = min;
    rtc_tm.tm_sec = sec;

    printf("write rtc %02d%02d%02d%02d%d.%02d",rtc_tm.tm_mon,rtc_tm.tm_mday,rtc_tm.tm_hour,rtc_tm.tm_min,rtc_tm.tm_year,rtc_tm.tm_sec);

    buf[0] = BIN2BCD(rtc_tm.tm_sec);
    buf[1] = BIN2BCD(rtc_tm.tm_min);
    buf[2] = BIN2BCD(rtc_tm.tm_hour);
    buf[3] = 0x01;             				//由于设置时间时不用星期这个项，故每次设置时默认为星期一
    buf[4] = BIN2BCD(rtc_tm.tm_mday);
    buf[5] = BIN2BCD(rtc_tm.tm_mon);
    buf[6] = BIN2BCD(rtc_tm.tm_year);

    if (fd_i2c)
    {
         for (i = 0; i < 7; i++)
         {
              i2cdata[0] = i;
              i2cdata[1] = buf[i];
              if (write(fd_i2c,&i2cdata[0], 2) != 2)
                {
                    printf("Write error at %d\n", i);
                    return -1;
                }
         }
    }
  return 0;
}

int setTime(unsigned char * time_buf)
{
    int ret;
    char rtc_string[40];
    ret = set_time(time_buf[0],time_buf[1],time_buf[2],time_buf[3],time_buf[4],time_buf[5],time_buf[6]);
    sprintf(rtc_string,"date %02d%02d%02d%02d%02d%02d.%02d",time_buf[2],time_buf[3],time_buf[4],time_buf[5],time_buf[0],time_buf[1],time_buf[6]);
    system(rtc_string);
    return ret;
}


int init_at24c02b()
{
    int ret;

    //fd_i2c0 = open("/dev/ttyS1", O_RDWR);
    fd_i2c0 = open(EEPROM, O_RDWR );//| O_NDELAY);

    if (fd_i2c0 < 0)
    {
        printf("\nfd_i2c0 = %d\n", fd_i2c0);
        perror("Can't open /dev!\n");
        return -1;
    }
    else
    {
        ret = ioctl(fd_i2c0, 0x0703, 0x50);	//首先设置at24c02b的地址
        if (ret < 0)
            perror("-------set at24c02b address error---");

        write_at24c02b(0,1);
        write_at24c02b(1,2);
        write_at24c02b(2,3);

        if(read_at24c02b(0) == 1 && read_at24c02b(1) == 2 && read_at24c02b(2) == 3)
        {
            printf("init at24c02b OK!\n");
            return 0;
        }
        else
        {
            printf("init at24c02b error!\n");
            return -1;
        }

    }

}

int read_at24c02b(unsigned int addr)
{
    unsigned char buf[2];

    if(fd_i2c0)
    {
        buf[1] = addr & 0xFF;
        buf[0] = (addr >> 8)&0xFF;
        if(write(fd_i2c0, buf, 2) != 2)		//first:write time address
        {
            printf("write error\n");
            return -1;
        }
        if (read(fd_i2c0, buf, 1) != 1) 	//second:read data
        {
            printf("read error\n");
            return -1;
        }
        return buf[0];
    }
    return -1;

}

int write_at24c02b(unsigned int addr, unsigned char data)
{
    unsigned char i2cdata[3];
    i2cdata[1] = addr & 0xFF;
    i2cdata[0] = (addr >> 8) & 0xFF;
    i2cdata[2] = data;



    if(write(fd_i2c0, &i2cdata[0], 3) != 3)
    {
        printf("write error\n");
        return -1;
    }
    return 0;

}

