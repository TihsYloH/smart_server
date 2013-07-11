#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**********************************************************************
* �������ƣ� GetNetStat
* ���������� ������������Ƿ�Ͽ�
* ���������
* ��������� ��
* �� �� ֵ�� ��������1,�Ͽ�����-1
* ����˵���� ��������Ҫ�����û�Ȩ�޲��ܳɹ�����ifconfig����
* �޸�����        �汾��     �޸���          �޸�����
* ---------------------------------------------------------------------
* 2010/04/02      V1.0      eden_mgqw
***********************************************************************/
int GetNetStat( )
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;

    memset( buffer, 0, BUFSIZ );
    read_fp = popen("ifconfig eth0 | grep RUNNING", "r");
    if ( read_fp != NULL )
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0)
        {
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }

    return ret;
}

int netConfigure()
{
    system("ifconfig eth0 up");
    system("ifconfig eth0 down");
    system("insmod /usb/rt3070sta.ko");
    system("ifconfig ra0 up");
    system("/usb/sbin/iwconfig ra0 essid \"TP\"");
    system("ifconfig ra0 192.168.1.99 netmask 255.255.255.0");
    system("mount -t nfs -o nolock 192.168.1.102:/home/ko/stream /var");
    return 0;
}



