/*
 * test_lock.c
 *
 *  Created on: 2013��7��11��
 *      Author: Administrator
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <mqueue.h>
#include <fcntl.h>

void run(sem_t * psem)
{
	while (1) {
		if (sem_wait(psem) < 0)
			printf("sem_wait");
		else
			printf("\n3����������\n");
	}
}
void run_nsep(sem_t * psem)
{
	printf("\n4����������\n");

	while (1) {
		if (sem_wait(psem) < 0)
			printf("sem_wait");
		else
			printf("\n4����������\n");
	}
}




int file_lock(int fd)
{
	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(fd, F_SETLKW, &lock) < 0) {
		perror("\nfcntl\n");
		return -1;
	}
	return 0;

}

int file_unlock(int fd)
{
	struct flock lock;
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(fd, F_SETLKW, &lock) < 0) {
		perror("\nfcntl\n");
		return -1;
	}
	return 0;

}


int run_fork5(int fd)
{
	printf("\n5 �Ž���\n");
	while (1) {
		if (file_lock(fd) < 0) {
			perror("\nfile_lock\n");
			continue;
		}
		printf("\n5�Ż����\n");
		sleep(10);
		if (file_unlock(fd) < 0) {
					perror("\nfile_unlock\n");
					continue;
		}
		usleep(1500000);

	}
}


int run_fork6(int fd)
{
	printf("\n6�Ž���\n");
	while (1) {
		if (file_lock(fd) < 0) {
			perror("\nfile_lock\n");
			continue;
		}
		printf("\n6�Ż����\n");
		sleep(1);
		if (file_unlock(fd) < 0) {
					perror("\nfile_unlock\n");
					continue;
		}
		sleep(1);
	}
}

int main(int argc, char ** argv)
{
	FILE * fp;
	void * dptr;
	int fd;
	pid_t pid;
	int fd_lock;

	printf("\n test lock\n");

	sem_t * pnsem;
#ifdef _POSIX_THREAD_PROCESS_SHARED
	printf("\n posix shared lock ok!\n");
#else
	printf("\n posix shared disabled\n");
#endif
	sem_t * psem;
	pthread_mutex_t *plock;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) < 0) {
		printf("\n set process shared failed\n");
	}


	fp = fopen("tmp_file", "r+");  //fopen:��һ���ļ����ļ�˳���򿪺�ָ��������ļ�ָ��ͻᱻ���أ�����ļ���ʧ���򷵻�NULL�����Ѵ���������errno ��
	if (fp == NULL)
	        {
	            fp = fopen("tmp_file", "w+");
	            if (fp == NULL)
	                return -1;

	            ftruncate(fileno(fp), sizeof(pthread_mutex_t)*10240);
	        }
	 fclose(fp);

	 while ((fd = open("tmp_file", O_RDWR)) < 0) {
	             if (errno == EINTR)
	                 continue;
	             perror("open");
	             return -1;
	 }

	 dptr = mmap(NULL, sizeof(pthread_mutex_t) + sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	 if (dptr == NULL || dptr == MAP_FAILED) {
		 printf("\n mmap fail\n");
		 return 1;
	 }

	 plock = dptr;

	 if (pthread_mutex_init(plock, &attr) < 0) {
		 printf("\npthread_mutex_init\n");
		 return 1;
	 }

	 psem = sem_open("/test_sem.sem", O_CREAT, O_RDWR, 1);
	 if (psem == SEM_FAILED) {
		 perror("\n sem_open \n");
		 return -1;
	 }

	 pnsem = dptr + sizeof(*plock);

	 if (sem_init(pnsem, 1, 0) < 0) {
		 printf("\nsem_init\n");
		 return -1;
	 }



	 fp = fopen("tmp_fd_lock", "r+");  //fopen:��һ���ļ����ļ�˳���򿪺�ָ��������ļ�ָ��ͻᱻ���أ�����ļ���ʧ���򷵻�NULL�����Ѵ���������errno ��
	 	if (fp == NULL)
	 	        {
	 	            fp = fopen("tmp_fd_lock", "w+");
	 	            if (fp == NULL)
	 	                return -1;

	 	            ftruncate(fileno(fp), sizeof(pthread_mutex_t)*10240);
	 	        }
	 	 fclose(fp);

	 	 while ((fd = open("tmp_fd_lock", O_RDWR)) < 0) {
	 	             if (errno == EINTR)
	 	                 continue;
	 	             perror("open");
	 	             return -1;
	 	 }



	 pid = fork();

	 if (pid < 0) {
		 printf("\n fork error\n");
		 return 1;
	 }
	 if (pid == 0) {	//�ӽ���
		 int i = 0;
		 int * ptr = NULL;
		 do {
			 pthread_mutex_lock(plock);
			 printf("\n �ӽ�����������	\n");
			 sleep(1);
			 i++;
			 if (i  == 3) {
				 printf("\n�ӽ����˳�\n");

			 }
			 sem_post(psem);
			 sem_post(pnsem);
			 pthread_mutex_unlock(plock);

		 }
		 while (1);
	 } else if (pid > 0) {	//������

		 pid = fork();

		 if (pid < 0) {
			 printf("\nfork\n");

		 }
		 if (pid == 0)
			 run(psem);
		 else {
			 pid = fork();
			 if (pid < 0) {
			 			 printf("\nfork\n");

			 		 }
			if (pid == 0)
			 	run_nsep(pnsem);
			else
				pid = fork();
				if (pid == 0)
					run_fork5(fd);
				else {
					pid = fork();
					if (pid == 0)
						run_fork6(fd);
				}
				sleep(2);
				while (1) {
				 pthread_mutex_lock(plock);
				 printf("\n��������������\n");
				 pthread_mutex_unlock(plock);
			 }
		 }
	 }


	pthread_mutexattr_destroy(&attr);
	sem_unlink("/tmp/test_sem");
	return 0;
}

