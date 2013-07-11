#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#define _DEBUG_

/*
int sqlite3_exec(sqlite3*, const char *sql, sqlite3_callback, void *, char **errmsg );

exec �ص�
int LoadMyInfo( void * para, int n_column, char ** column_value, char ** column_name )
{
//para������ sqlite3_exec �ﴫ��� void * ����
//ͨ��para����������Դ���һЩ�����ָ�루������ָ�롢�ṹָ�룩��
Ȼ����������ǿ��ת���ɶ�Ӧ�����ͣ���������void*���ͣ�����ǿ��ת����������Ͳſ��ã���Ȼ�������Щ����
//n_column����һ����¼�ж��ٸ��ֶ� (��������¼�ж�����)
// char ** column_value �Ǹ��ؼ�ֵ������������ݶ������������ʵ�����Ǹ�1ά���飨��Ҫ��Ϊ��2ά���飩��
ÿһ��Ԫ�ض���һ�� char * ֵ����һ���ֶ����ݣ����ַ�������ʾ����/0��β��
//char ** column_name �� column_value�Ƕ�Ӧ�ģ���ʾ����ֶε��ֶ�����
//����Ҳ�ʹ�� para �������������Ĵ���.
typedef int (*sqlite3_callback)(void*,int,char**, char**);

  */

int callb_back(void * arg, int n_column, char ** column_value, char ** column_name)
{
    int i;

    printf("\n****************************************************************\
           ***********************************************************\n");
    return 0;

}

int call_back(void * db, int n_column, char ** column_value, char ** column_name)
{
    int i;
    char * sql;
    char * zErrMsg;
    printf("\nn_column = %d\n", n_column);
    printf("\n");
    for (i = 0; i < n_column; i++) {
       printf("%s = %s   ", column_name[i], column_value[i]);
    }

    sql = "SELECT * FROM SensorData";

    sqlite3_exec( db , sql ,  callb_back, 0 , &zErrMsg );

    printf("\n");
    return 0;

}

int main( void )
{
    sqlite3 *db=NULL;
    char *zErrMsg = 0;
    int rc;
    rc = sqlite3_open("zieckey.db", &db); //��ָ�������ݿ��ļ�,��������ڽ�����һ��ͬ�������ݿ��ļ�
    if( rc )
    {
      fprintf(stderr, "Can't open database: %s/n", sqlite3_errmsg(db));
      sqlite3_close(db);
      exit(1);
    }
    else
        printf("You have opened a sqlite3 database named zieckey.db successfully!/nCongratulations! Have fun !  ^-^ \n");
    //����һ����,����ñ���ڣ��򲻴�������������ʾ��Ϣ���洢�� zErrMsg ��
    char *sql = " CREATE TABLE SensorData(ID INTEGER PRIMARY KEY,\
            SensorID INTEGER, \
            SiteNum INTEGER, \
            Time VARCHAR(12), \
            SensorParameter REAL );";
    sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
    #ifdef _DEBUG_
            printf("%s ssss\n",zErrMsg);
    #endif
    //��������
    char* sql1 = "INSERT INTO SensorData VALUES( NULL , 2 , 3 , '200605011206', 18.9 );" ;
    sqlite3_exec( db , sql1 , 0 , 0 , &zErrMsg );
#ifdef _DEBUG_
        printf("%s ssss\n",zErrMsg);
#endif
    char* sql2 = "INSERT INTO SensorData VALUES( NULL , 4 , 5 , '200605011306', 16.4 );" ;
    sqlite3_exec( db , sql2 , 0 , 0 , &zErrMsg );
#ifdef _DEBUG_
        printf("%s ssss\n",zErrMsg);
#endif

/*              ����                    */
     int nrow = 0, ncolumn = 0;
     char **azResult; //��ά�����Ž��

      sql = "SELECT * FROM SensorData";

      sqlite3_exec( db , sql ,  call_back, db , &zErrMsg );
      /*
      sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
      int i = 0 ;
      printf( "row:%d column=%d \n" , nrow , ncolumn );
     printf( "\nThe result of querying is : \n" );

      for( i=0 ; i<( nrow + 1 ) * ncolumn ; i++ ) {
       printf( "azResult[%d] = %s ", i , azResult[i] );
       if (i % 5 == 0 && i != 0);
           printf("\n");
      }
      //�ͷŵ�  azResult ���ڴ�ռ�
      sqlite3_free_table( azResult );
      */
      #ifdef _DEBUG_
             printf("zErrMsg = %s \n", zErrMsg);
      #endif


/*           ɾ��            */
             //char *sql = " CREATE TABLE SensorData(ID INTEGER PRIMARY KEY, SensorID INTEGER, SiteNum INTEGER, Time VARCHAR(12), SensorParameter REAL);" ;
             sql = "DELETE FROM SensorData WHERE SensorID = 1 ;" ;





    sqlite3_close(db); //�ر����ݿ�
    printf("\n\n");
    return 0;
}
