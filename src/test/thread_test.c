#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sched.h>
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
#include <assert.h>
typedef void *(*ptexec)(void *arg);
int WorkThreadCreate(ptexec threadexec, int prio, int pol)
{
	pthread_t pid;
	pthread_attr_t attr;
	struct sched_param param;
	//int policy;
	param.sched_priority = 50;

	int err;
														//..................1M.................
	err = pthread_attr_init(&attr);								//........................
	int inher; //= PTHREAD_EXPLICIT_SCHED;
	inher = PTHREAD_EXPLICIT_SCHED;
	err = pthread_attr_setinheritsched(&attr, inher);
	err = pthread_attr_setschedpolicy(&attr, SCHED_RR);			//SCHED_FIFO --.....SCHED_RR--....SCHED_OTHER--..

	err = pthread_attr_setschedparam(&attr, &param);
	if (err != 0)
	{
		printf("\n---------------------\n------\n---\n");
	}
//	err = pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);

	if (err != 0 )
	{
		printf("\n\n\n\n");
	}
	//err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);			//.........
																	//PTHREAD_CREATE_DETACHED -- ....
																	//PTHREAD _CREATE_JOINABLE -- .....
	if (err == 0)
	{
		err = pthread_create(&pid, &attr, threadexec, NULL); //(void*)&sttConnSock[i]);;
		if (err != 0)
		{
			perror("\n----WorkThreadCreate--pthread_create err\n");
			return err;
		}
	}
	err = pthread_attr_destroy(&attr);
	if (err != 0)
	{
		perror("\n----WorkThreadCreate--pthread_attr_destroy err\n");
		return err;
	}
	return 0;
}
void *test1(void *arg)
{
	arg = arg;
	while (1){ 
		printf("\n----test1 is running now-------------\n");
	}
}
void delay(int *times)
{
	*times = *times;
	return;
}
void delay_time(int times)
{
	while(times != 0){
		times--;
		delay(&times);
	}
	return;
}
void *test2(void *arg)
{
	
	while(1){
		delay_time(10000);

	}
}
static int get_thread_policy(pthread_attr_t *attr);

static void show_thread_priority(pthread_attr_t *attr,int policy);
void *test3(void *arg)
{
	pthread_attr_t attr;
	struct sched_param param;
	pthread_attr_init(&attr);
	while(1) {
		int policy = get_thread_policy(&attr);
	        printf("\n-- test3 ead show defalut pol\n");
	        show_thread_priority(&attr, policy);
		pthread_getschedparam(pthread_self(), &policy, &param);
printf("\n--now priority is %d\n", param.sched_priority);
		printf("\npolicy = %d, param = %d\n", policy, param.sched_priority);
	sleep(1);	
		printf("\n-----test 3 running----\n");
	}
}

static int get_thread_policy(pthread_attr_t *attr)
{
  int policy;
  int rs = pthread_attr_getschedpolicy(attr,&policy);
  assert(rs==0);
  switch(policy)
  {
  case SCHED_FIFO:
    printf("policy= SCHED_FIFO\n");
    break;
  case SCHED_RR:
    printf("policy= SCHED_RR");
    break;
  case SCHED_OTHER:
    printf("policy=SCHED_OTHER\n");
    break;
  default:
    printf("policy=UNKNOWN\n");
    break;
  }
  return policy;
}

static void show_thread_priority(pthread_attr_t *attr,int policy)
{
  int priority = sched_get_priority_max(policy);
  assert(priority!=-1);
  printf("max_priority=%d\n",priority);
  priority= sched_get_priority_min(policy);
  assert(priority!=-1);
  printf("min_priority=%d\n",priority);
}

static int get_thread_priority(pthread_attr_t *attr)
{
  struct sched_param param;
  int rs = pthread_attr_getschedparam(attr,&param);
  assert(rs==0);
  printf("priority=%d",param.__sched_priority);
  return param.__sched_priority;
}

static void set_thread_policy(pthread_attr_t *attr,int policy)
{
  int rs = pthread_attr_setschedpolicy(attr,policy);
  assert(rs==0);
  get_thread_policy(attr);
}
int main(int argc, char **argv)
{
	pthread_t id;
	printf("\nSCHED_OTHER = %d, SCHED_FIFO = %d, SCHED_RR = %d---\n", SCHED_OTHER, SCHED_FIFO, SCHED_RR);
	//pthread_create(&id, NULL, test1, NULL);
	WorkThreadCreate(test2, 80, SCHED_FIFO);
	WorkThreadCreate(test3, 70, SCHED_FIFO);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	int policy = get_thread_policy(&attr);
	printf("\n--main thread show defalut pol\n");
	show_thread_priority(&attr, policy);

	while(1);
	return 1;
}


