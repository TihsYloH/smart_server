#include "mempool.h"
#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <sys/time.h>
#include <pthread.h>

int mkk;

static int searchForpos(int *value, int high, int dstValue)
{
    int low = 0;
    int tmp;
    high--;
    while (high - low > 1) {
        printf("\nhigh = %d, low = %d\n", high, low);
        scanf("%d", &tmp);
        if (dstValue < value[(high - low)/2]) {
            printf("\n  < dstValue = %d, value[(high - low)/2] = %d\n", dstValue, value[(high - low)/2]);
            high = (high+low)/2;
        }
        else if (dstValue > value[(high - low)/2]) {
            printf("\n  > dstValue = %d, value[(high - low)/2] = %d\n", dstValue, value[(high - low)/2]);
            low = (high+low)/2;
        }
        else
            return (high + low)/2;

    }
    if (dstValue > value[low])
        return high;
    return low;
}

pthread_mutex_t xxxx = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
    char * ptr;
    int i, j;
    struct timeval tv1, tv2;
    //int j;
    //scanf("%d", &j);
   // //int s[8] = {10, 30, 59, 88, 89, 90, 99, 150};
   // i = searchForpos(s, 8, j);
    ////printf("\ni = %d\n", i);
   // return 0;

    mempool_init(20, 4, 5, 1);
    printf("\nmempool iiiiiiiiiiiiiiiiiiiiiii\n");

    ptr = m_malloc(20);
    printf("\naaaa\n");
    ptr = m_realloc(ptr, 1300);
    printf("\naaaa\n");
    m_free(ptr);


    ptr = m_malloc(1500);



    printf("\n 20 ptr = %p\n", ptr);

    //m_free(ptr);

    ptr = m_malloc(20);

    printf("\n20  ptr = %p\n", ptr);

    ptr = m_realloc(ptr, 20);

    printf("\n20  reallo cptr = %p\n", ptr);

    ptr = m_malloc(19);

    printf("\n 19 ptr = %p\n", ptr);

    ptr = m_malloc(50);

    printf("\n 50 ptr = %p\n", ptr);

    ptr = m_malloc(150);

    printf("\n 150 ptr = %p\n", ptr);


    printf("\nm_realloc 140\n");
    ptr = m_realloc(ptr, 140);

    printf("\n 150 ptr = %p\n", ptr);

    ptr = m_realloc(ptr, 200);

    printf("\n 200 ptr = %p\n", ptr);
    printf("\ncount =\n");
    ptr = m_realloc(ptr, 1300);
    m_free(ptr);

    ptr = m_realloc(ptr, 1800);
    m_free(ptr);

    printf("\n 400 ptr = %p\n", ptr);
    printf("mempool_destroy(); = %d\n", mempool_destroy());
    for (j = 0; j < 100; j++) {
        sleep(1);
        gettimeofday(&tv1, NULL);
        for (i = 0; i < 40000; i++) {
            ptr = m_malloc(1000);
            m_free(ptr);
        }
        gettimeofday(&tv2, NULL);

        printf("\n---m_malloc: %d   %d\n", tv2.tv_sec-tv1.tv_sec, tv2.tv_usec-tv1.tv_usec);

        gettimeofday(&tv1, NULL);

        for (i = 0; i < 40000; i++) {

            ptr = malloc(1000);



            free(ptr);

        }
        gettimeofday(&tv2, NULL);

        printf("\n****malloc: %d   %d\n", tv2.tv_sec-tv1.tv_sec, tv2.tv_usec-tv1.tv_usec);
    }

    return 0;
    while (1) {
        printf("\nfree test\n");
        mkk = 1;
        ptr = m_malloc(1000);

        for ( i = 0; i < 1000; i++)
            ptr[i] = i;

        if (ptr == NULL)
            return 0;
        m_free(ptr);

        ptr = m_malloc(50);

        for ( i = 0; i < 50; i++)
            ptr[i] = i;

        ptr = m_realloc(ptr, 1200);
        for ( i = 0; i < 1200; i++)
            ptr[i] = i;
        m_free(ptr);
    }

    return 0;
}
