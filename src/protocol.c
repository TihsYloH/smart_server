#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"



uint32_t cal_check(uint8_t *value, uint32_t size)
{
    uint32_t total = 0;
    int i = 0;
    while (i < size)
        total += value[i++];
    return total&0xFF;
}


uint64_t getaddr_byte(uint8_t * pdata, int len)
{
    uint64_t dat = 0;
    if (len < 8)
        return 0;
    int i;
    for (i = 0; i < 8; i++) {
        dat += ((uint64_t)pdata[i] << (i << 3));
    }
    return dat;
}
