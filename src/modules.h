#ifndef MODULES_H
#define MODULES_H
#include <stdint.h>
#include "events.h"

#define MAX_MODULE_NO       15

#define   MAX_CMD_WORDS  (UPLOAD+1)


/*
#define     MODULE_CTRL              1
#define     MODULE_QUERY             0
#define     MODULE_RED               2
#define     MODULE_LOCAL             3
#define     MODULE_MIX               4
*/
typedef void * (*zigmod_handler)(sqlite_db_t *sqlite, void *, void *, int);

typedef struct zig_mod {
    uint8_t mod_no;
    //uint8_t mod_type;
    event_level_t level;
   // event_level_t level;
    zigmod_handler net_pt[3];
    zigmod_handler serial_pt[3];
} zig_mod_t;




typedef struct ir_mod {
    uint8_t mod_no;
    uint8_t innet_addr;
    uint64_t ext_addr;
} ir_mod_t;



uint8_t * getstrbytime(long t, uint8_t str[], int n);

long gettimebystr(uint8_t timer[], int n);

extern const zig_mod_t   g_zig_mod[];

#endif // MODULES_H
