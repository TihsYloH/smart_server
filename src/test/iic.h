/****************************************************************************

 *                                                                          *

 * Copyright (c) 2004 - 2006 Winbond Electronics Corp. All rights reserved. *

 *                                                                          *

 ****************************************************************************/

/*********************************************************************************

 *

 * FILENAME

 *     iic.h

 *

 * VERSION

 *     1.0

 *

 * DESCRIPTION

 *     Library for RTC

 *

 * DATA STRUCTURES

 *     None

 *

 * FUNCTIONS

 *     None

 *

 * HISTORY

 *     09/05/2005            Ver 1.0 Created by PC34 YHan

 *

 * REMARK

 *     None

 **************************************************************************/


#ifndef _IIC_H
#define _IIC_H





/*************** RTC *************************/

extern int init_ds3231();


/***************************************/
/*    time_buf->20121122100522         */
/***************************************/
extern int setTime(unsigned char * time_buf);


extern void getTime(struct tm *tm);


/*************** EEPROM *********************/
/*     addr ×Ö½ÚµØÖ· bit                         */
/*********************************************/

extern int init_at24c02b();
extern int read_at24c02b(unsigned int addr);
extern int write_at24c02b(unsigned int addr, unsigned char data);
/*
eeprom  table
addr      fun            value
0 -  2    eeprom_check   1 2 3
1 - 11    web_par
30        ip_en          11
31 - 34   ipdz
35 - 38   zwym
39 - 42   mywg
45        led_en         11
46        led_on
*/

#endif
