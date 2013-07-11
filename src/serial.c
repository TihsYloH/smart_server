#include     <stdio.h>      /*标准输入输出定义*/
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix 标准函数定义*/
#include     <sys/types.h>  
#include     <sys/stat.h>   
#include     <fcntl.h>      /*文件控制定义*/
#include     <termios.h>    /*PPSIX 终端控制定义*/
#include     <errno.h>      /*错误号定义*/

#define FALSE	0
#define TRUE	1

static speed_t speed_arr[] = { B38400, B19200, B9600, B4800, B2400, B1200, B300,
                                        B38400, B19200, B9600, B4800, B2400, B1200, B300, };
static int name_arr[] = {38400,  19200,  9600,  4800,  2400,  1200,  300, 38400,  
                                        19200,  9600, 4800, 2400, 1200,  300, };
static void set_speed(int fd, int speed){
        int   i; 
        int   status; 
        struct termios   Opt;
        //speed_t temp;
        tcgetattr(fd, &Opt); 
        for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) { 
                if  (speed == name_arr[i]) {     
                        tcflush(fd, TCIOFLUSH);     
                        cfsetispeed(&Opt, speed_arr[i]);  
                        cfsetospeed(&Opt, speed_arr[i]);   
                        status = tcsetattr(fd, TCSANOW, &Opt);  
                        if  (status != 0) {        
                                perror("tcsetattr fd");  
                                return;     
                        }    

                        // tcgetattr(fd, &Opt);
                        // temp = cfgetispeed(&Opt);
                       // printf("\nspeed input is %d\n", temp);

                        // temp = cfgetospeed(&Opt);
                       //  printf("\nspeed output is %d\n", temp);

                        tcflush(fd,TCIOFLUSH);   
                }  
        }
}
//cfgetispeed和cfgetospeed

static int set_Parity(int fd,int databits,int stopbits,int parity)
{ 
        struct termios options; 
        if  ( tcgetattr( fd,&options)  !=  0) { 
                perror("SetupSerial 1");     
                return(FALSE);  
        }
        options.c_cflag &= ~CSIZE; 

        options.c_cflag |= CLOCAL | CREAD;
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag &= ~OPOST;
        options.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        switch (databits) /*设置数据位数*/
        {   
        case 7:                
                options.c_cflag |= CS7; 
                break;
        case 8:     
                options.c_cflag |= CS8;
                break;   
        default:    
                fprintf(stderr,"Unsupported data size\n"); return (FALSE);  
        }
		switch (parity) 
		{   
        case 'n':
        case 'N':    
                options.c_cflag &= ~PARENB;   /* Clear parity enable */
                options.c_iflag &= ~INPCK;     /* Enable parity checking */ 
                break;  
        case 'o':   
        case 'O':     
                options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
                options.c_iflag |= INPCK;             /* Disnable parity checking */ 
                break;  
        case 'e':  
        case 'E':   
                options.c_cflag |= PARENB;     /* Enable parity */    
                options.c_cflag &= ~PARODD;   /* 转换为偶效验*/     
                options.c_iflag |= INPCK;       /* Disnable parity checking */
                break;
        case 'S': 
        case 's':  /*as no parity*/   
            options.c_cflag &= ~PARENB;
                options.c_cflag &= ~CSTOPB;break;  
        default:   
                fprintf(stderr,"Unsupported parity\n");    
                return (FALSE);  
        }  
		/* 设置停止位*/  
		switch (stopbits)
		{   
		        case 1:    
		                options.c_cflag &= ~CSTOPB;  
		                break;  
		        case 2:    
		                options.c_cflag |= CSTOPB;  
		           break;
		        default:    
		                 fprintf(stderr,"Unsupported stop bits\n");  
		                 return (FALSE); 
		} 
		/* Set input parity option */ 
		if (parity != 'n')   
		        options.c_iflag |= INPCK; 
		tcflush(fd,TCIFLUSH);
		options.c_cc[VTIME] = 150; /* 设置超时15 seconds*/   
		options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
        options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);  /*Input*/
        options.c_oflag  &= ~OPOST;   /*Output*/
		
		if (tcsetattr(fd,TCSANOW,&options) != 0)   
		{ 
		        perror("SetupSerial 3");   
		        return (FALSE);  
		} 
		return (TRUE);  
}



static int OpenDev(char *Dev)
{
        int        fd = open( Dev, O_RDWR | O_NDELAY );         //| O_NOCTTY | O_NDELAY
        if (-1 == fd)        
        {                         
                perror("Can't Open Serial Port");

                return -1;                
        }        
        else        
                return fd;

}
/*
static void SetNonBlock(int fdListen)
{
    int opts;
    if ((opts = fcntl(fdListen, F_GETFL)) < 0)
    {
        perror("fcntl(F_GETFL) error\n");
        exit(1);
    }
    opts |= O_NONBLOCK;
    if (fcntl(fdListen, F_SETFL, opts) < 0)
    {
        perror("fcntl(F_SETFL) error\n");
        exit(1);
    }
}
*/

int init_serial(char *dev, int speed)
{
	int fd;

	fd = OpenDev(dev);

	if (fd < 0)
		return -1;
	set_speed(fd, speed);
	
	if (set_Parity(fd,8,1,'N') == FALSE)  {
    	printf("Set Parity Error\n");
        return -1;
    }
	return fd;
}
/*
int main(int argc, char **argv){
        int fd;
        int nread;
		int nwrite;
        char buff[512];
		char test[512]= "test start\n";
        char *dev  = "/dev/ttyS1"; //串口二
        fd = OpenDev(dev);
        set_speed(fd,38400);
        if (set_Parity(fd,8,1,'N') == FALSE)  {
                printf("Set Parity Error\n");
                exit (0);
        }
        //SetNonBlock(fd);
        while (1) //循环读取数据
        {
            //printf("\nnwrite = %d\n", 12);
            nwrite = write(fd, test, 12);
            printf("\nnwrite = %d\n", 12);
            do
            {

                    nread = read(fd, buff, 512);

                    if (nread < 0) {
                        printf("\nread err\n");
                        break;
                    }
                    if (nread == 0)
                        continue;
                    nwrite = write(fd, buff, nread);
                    printf("\nLen %d\n",nread);
                    buff[nread+1] = '\0';
                    printf("\nbuff first:%d\n", buff[0]);
                    printf( "\n%s", buff);
                    break;
            }
            while (1);
        }
        //close(fd);  
        // exit (0);
}
*/
