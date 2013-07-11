#include <stdlib.h>
#include <stdio.h>
#include "save_wave.h"

int main(int argc, char ** argv)
{
    uint8_t pdata[1000];
    uint8_t data[1000];
    int size = 1000;
    char * t;
    char * s;
    int cnt;
    int i;

    for (i = 0; i < 1000; i ++)
        pdata[i] = i;

    wave_init();

    if (wave_add(OPEN_SHIFT, pdata, 1000) < 0)
        return -1;

    t = wave_time(OPEN_SHIFT, &cnt);


    printf("\ncnt = %d\n", cnt);
    i = 0;
    while((s = get_wtime(t, i, cnt)) != NULL) {
        i++;
        printf("\nt = %s\n", t);
        size = 1000;
        if (wave_search(OPEN_SHIFT, t, data, &size) >= 0) {
            int j = 0;
            printf("\nsize = %d\n", size);
            for (j = 0; j < size; j++) {
                printf("%d  ", data[j]);
            }
        }

        t += RTC_LEN;
    }

    wave_destroy();

    printf("\n\nhellow\n\n");
    return 0;
}
