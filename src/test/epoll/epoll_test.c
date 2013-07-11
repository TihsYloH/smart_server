#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <asm/types.h>
#include <netinet/ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


/************epoll API*********************/
/*
int epoll_create(int size);

//op
EPOLL_CTL_ADD：        注册新的fd到epfd中；
EPOLL_CTL_MOD：       修改已经注册的fd的监听事件；
EPOLL_CTL_DEL：        从epfd中删除一个fd；
//epoll_event *event
struct epoll_event {
                 __uint32_t events;
                epoll_data_t data;
}
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);


//timeout 毫秒
//maxevents < size
int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);

EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
EPOLLOUT：表示对应的文件描述符可以写；
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
EPOLLERR：表示对应的文件描述符发生错误；
EPOLLHUP：表示对应的文件描述符被挂断；
EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
*/
/*********************************************/


void setnonblocking(int sock)
{
    int opts;
    opts=fcntl(sock,F_GETFL);
    if(opts<0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    struct epoll_event ev, events[20];

    struct sockaddr_in clientAddr;
    struct sockaddr_in serverAddr;

    int nfds, max, epfd, i, connFd, clilen;
    int portnumber = 7000;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

      epfd = epoll_create(256);

    ev.data.fd = listenFd;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenFd, &ev);

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    char *local_addr = "192.168.10.131";

    inet_aton(local_addr, &(serverAddr.sin_addr));

    serverAddr.sin_port = htons(portnumber);

    bind(listenFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenFd, 5);
    while (1) {
        nfds = epoll_wait(epfd, events, 20, 500);
        for (i = 0; i < nfds; i++) {
            if (events[i].data.fd == listenFd) {
                printf("\nevents[i].data.fd == listenFd\n");
                connFd = accept(listenFd, (struct sockaddr *)&clientAddr, &clilen);
                if (connFd < 0) {
                    perror("accept");
                    exit(1);
                }

                ev.data.fd = connFd;

                ev.events=EPOLLIN|EPOLLET|EPOLLPRI|EPOLLHUP;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connFd, &ev);
                continue;
            }
            if (events[i].events&EPOLLIN) {
                printf("\n---EPOLLIN---\n");
                if (events[i].data.fd < 0) {
                    perror("events[i].data.fd < 0");
                    continue;
                }
            }
            if (events[i].events&EPOLLPRI) {
                printf("\n---EPOLLPRI---\n");
                if (events[i].data.fd < 0) {
                    perror("events[i].data.fd < 0");
                    continue;
                }
            }
            if (events[i].events&EPOLLHUP) {
                printf("\n---EPOLLHUP---\n");
                if (events[i].data.fd < 0) {
                    perror("events[i].data.fd < 0");
                    continue;
                }
            }
            if (events[i].events&EPOLLERR) {
                printf("\nEPOLLERR\n");
            }

        }
    }
    return 0;
}
