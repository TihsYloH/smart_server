#ifndef SEND_H
#define SEND_H

#include "smt_ev.h"

int send_init(void);

void send_destroy(void);

int send_register(int fd, void * arg, smtFileProc proc, smt_fd_events * pevents);

void send_unregister(smt_fd_events * pevents);

void send_loop(void);




#endif // SEND_H
