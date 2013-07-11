#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#define _DEBUG_

/*
int sqlite3_exec(sqlite3*, const char *sql, sqlite3_callback, void *, char **errmsg );

exec 回调
int LoadMyInfo( void * para, int n_column, char ** column_value, char ** column_name )
{
//para是你在 sqlite3_exec 里传入的 void * 参数
//通过para参数，你可以传入一些特殊的指针（比如类指针、结构指针），
然后在这里面强制转换成对应的类型（这里面是void*类型，必须强制转换成你的类型才可用）。然后操作这些数据
//n_column是这一条记录有多少个字段 (即这条记录有多少列)
// char ** column_value 是个关键值，查出来的数据都保存在这里，它实际上是个1维数组（不要以为是2维数组），
每一个元素都是一个 char * 值，是一个字段内容（用字符串来表示，以/0结尾）
//char ** column_name 跟 column_value是对应的，表示这个字段的字段名称
//这里，我不使用 para 参数。忽略它的存在.
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
    rc = sqlite3_open("zieckey.db", &db); //打开指定的数据库文件,如果不存在将创建一个同名的数据库文件
    if( rc )
    {
      fprintf(stderr, "Can't open database: %s/n", sqlite3_errmsg(db));
      sqlite3_close(db);
      exit(1);
    }
    else
        printf("You have opened a sqlite3 database named zieckey.db successfully!/nCongratulations! Have fun !  ^-^ \n");
    //创建一个表,如果该表存在，则不创建，并给出提示信息，存储在 zErrMsg 中
    char *sql = " CREATE TABLE SensorData(ID INTEGER PRIMARY KEY,\
            SensorID INTEGER, \
            SiteNum INTEGER, \
            Time VARCHAR(12), \
            SensorParameter REAL );";
    sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
    #ifdef _DEBUG_
            printf("%s ssss\n",zErrMsg);
    #endif
    //插入数据
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

/*              查找                    */
     int nrow = 0, ncolumn = 0;
     char **azResult; //二维数组存放结果

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
      //释放掉  azResult 的内存空间
      sqlite3_free_table( azResult );
      */
      #ifdef _DEBUG_
             printf("zErrMsg = %s \n", zErrMsg);
      #endif


/*           删除            */
             //char *sql = " CREATE TABLE SensorData(ID INTEGER PRIMARY KEY, SensorID INTEGER, SiteNum INTEGER, Time VARCHAR(12), SensorParameter REAL);" ;
             sql = "DELETE FROM SensorData WHERE SensorID = 1 ;" ;





    sqlite3_close(db); //关闭数据库
    printf("\n\n");
    return 0;
}
