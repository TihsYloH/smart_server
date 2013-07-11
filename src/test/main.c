#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <stdint.h>

uint8_t * getstrbytime(long t, uint8_t str[], int n)
{
    struct tm tm;
    if (n < 7)
        return NULL;
    if (localtime_r(&t, &tm) == NULL)
        return NULL;
    str[0] = 20;
    str[1] = tm.tm_year%100;
    str[2] = tm.tm_mon;
    str[3] = tm.tm_mday;
    str[4] = tm.tm_hour;
    str[5] = tm.tm_min;
    str[6] = tm.tm_sec;
    return str;
}

long gettimebystr(uint8_t timer[], int n)
{
    struct tm tm;

    if (n < 7)
        return -1;

    memset(&tm, 0, sizeof(tm));

    tm.tm_isdst = 1;

    tm.tm_year = 100 + timer[1];
    tm.tm_mon = timer[2];
    tm.tm_mday = timer[3];
    tm.tm_hour = timer[4];
    tm.tm_min = timer[5];
    tm.tm_sec = timer[6];

    /*
    tm.tm_year = 2013;
    tm.tm_mon = 5;
    tm.tm_mday = 19;
    tm.tm_hour = 15;
    tm.tm_min = 12;
    tm.tm_sec = 12;
    */
    return mktime(&tm);
}

void test_time(void)
{
    uint8_t str[10];
    printf("\n******test_time*******\n");
    long t = time(NULL);

    printf("\nutc:%ld\n", t);

    if (getstrbytime(t, str, 7) == NULL) {
        printf("\ntest_time\n");
        return;
    }
    int i;
    printf("\n");
    for (i = 0; i < 7; i++)
        printf("%d  ", str[i]);
    printf("\n");

    printf("\nnow utc = %ld\n", gettimebystr(str, 7));

}

int main(int argc, char **argv)
{
    test_time();


    time_t timep;
    struct tm *p;
    time(&timep);
    printf("time() : %d \n",timep);
    p=localtime(&timep);

    printf("\n*****   %d   %s  %ld\n", p->tm_isdst, p->tm_zone, p->tm_gmtoff);

    timep = mktime(p);
    printf("time()->localtime()->mktime():%d\n",timep);

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = p->tm_year;
    tm.tm_mon = p->tm_mon;
    tm.tm_hour = p->tm_hour;
    tm.tm_mday = p->tm_mday;
    tm.tm_min = p->tm_min;
    tm.tm_sec = p->tm_sec;

    printf("\n%d %d %d %d %d %d\n", p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

    /*
    tm.tm_year = 2011;
    tm.tm_mon = 11;
    tm.tm_mday = 12;
    tm.tm_hour = 12;
    tm.tm_min = 12;
    tm.tm_sec = 11;
*/
    printf("\nmktime = %ld\n", mktime(&tm));

	return 0;
}
