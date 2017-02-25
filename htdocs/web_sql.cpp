/*************************************************************************
	> File Name: test.cpp
	> Author: 
	> Mail: 
	> Created Time: 2017年02月20日 星期一 09时11分08秒
 ************************************************************************/

#include"web_sql.h"

const char* host = "localhost";
const char* user = "root";
const char* passwd ="nice";
const char *dbname = "Lab";


void query_sql(const char *sql, char *result){
    MYSQL conn; //数据库连接
    int res;   //执行sql语句后的返回标志
    MYSQL_RES *res_ptr;  //指向查询结果的指针
    MYSQL_FIELD *field; //字段结构指针
    MYSQL_ROW  result_row;  //按行返回的查询信息

    int row, column;  //查询返回的行数和列数
    int i, j;
    //初始化mysql连接
    mysql_init(&conn);

   
    const char* error_msg = "<DIV STYLE=\"COLOR:RED\">Error: mysql error!</DIV>\n";    
        
    //建立连接，返回不为空代表成功
    if(mysql_real_connect(&conn, host, user, passwd, dbname, 0, NULL, CLIENT_FOUND_ROWS))
    {

        //printf("query_sql success\n"); 
        
        //设置查询编码为utf8,这样支持中文
        mysql_query(&conn, "set names utf8");

        //用mysql_query函数来执行我们刚传入的sql语句
        //返回值为int,如果为0，代表成功，否则失败
        res = mysql_query(&conn, sql);

        if(res){
            memcpy(result, error_msg, strlen(error_msg));
           // printf("mysql_query fail\n");
            mysql_close(&conn);
            return;
        }else{
            //现在，代表查询成功
            res_ptr = mysql_store_result(&conn);

            //如果不为空，打印结果
            if(res_ptr){
                //取得结果的行数和列数
                column = mysql_num_fields(res_ptr);
                row = mysql_num_rows(res_ptr) + 1;
                
                //printf("查询到 %d 行\n", row );

                //输出结果的字段名
                /*for(i = 0;field = mysql_fetch_field(res_ptr); ++i)
                    printf("%s\t", field->name);
                printf("\n");
                */
                //按行输出结果
                int len = 0;
                char *head = result;
                char *index[20] = {"", "姓名： ", "年龄： ", "家乡： ", "电话： ", "标签： "};
                for(i = 1; i < row; ++i){
                    result_row = mysql_fetch_row(res_ptr);
                    for(j = 1; j < column; ++j)
                    {   
                        
                        char tmp[1024];   
                        sprintf(tmp, "<Div STYLE=\" COLOR:GREEN; font-size:30px; font-weight:bold\">%s %s </DIV>\n", index[j], result_row[j]);
                        len = strlen(tmp);
                        memcpy(head, tmp, len);
                        head += len;
                    }
                }
            }
            mysql_free_result(res_ptr);
            mysql_close(&conn);
            mysql_library_end();
        }
    }
    else{
        memcpy(result, error_msg, strlen(error_msg));
        printf( "mysql conn fail" );
        return ;
    }
}

void insert_sql(const char *sql){
    MYSQL conn;
    int res;

    mysql_init(&conn);

    if(mysql_real_connect(&conn, host, user, passwd, dbname, 0, NULL,CLIENT_FOUND_ROWS)){
        printf("connect success\n") ;

        res = mysql_query(&conn, sql);
        if(res){
            printf("mysql_query error\n" );
        }
        else{
            printf( "insert ok\n") ;
        }
        mysql_close(&conn);
        mysql_library_end();
    }
}

/*int main()
{
    query_sql("select * from student");

    return 0;
}*/
