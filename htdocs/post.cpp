/*************************************************************************
    > File Name: post.c
    > Author: 
    > Mail: 
    > Created Time: 2017年02月24日 星期五 09时23分11秒
            char m[10],n[10];
 ************************************************************************/
#include<wchar.h>
#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include<ctype.h>
#include"web_sql.h"
static unsigned char hexchars[] = "0123456789ABCDEF";  
  
static int php_htoi(char *s)  
{  
    int value;  
    int c;  
    
    c = ((unsigned char *)s)[0];  
    if (isupper(c))  
        c = tolower(c);  
    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;  
    
    c = ((unsigned char *)s)[1];  
    if (isupper(c))  
        c = tolower(c);  
    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;  
    
    return (value);  
}  
    
      
char *php_url_encode(char const *s, int len, int *new_length)  
{  
    register unsigned char c;  
    unsigned char *to, *start;  
    unsigned char const *from, *end;  
            
    from = (unsigned char *)s;  
    end  = (unsigned char *)s + len;  
    start = to = (unsigned char *) calloc(1, 3*len+1);  
        
    while (from < end)   
    {  
        c = *from++;  
            
        if (c == ' ')   
        {  
            *to++ = '+';  
        }   
        else if ((c < '0' && c != '-' && c != '.') ||  
                (c < 'A' && c > '9') ||  
                (c > 'Z' && c < 'a' && c != '_') ||  
                (c > 'z'))   
        {  
            to[0] = '%';  
            to[1] = hexchars[c >> 4];  
            to[2] = hexchars[c & 15];  
            to += 3;  
        }  
        else   
        {  
             *to++ = c;  
        }  
    }  
    *to = 0;  
    if (new_length)   
    {  
        *new_length = to - start;  
    }  
    return (char *) start;  
}  
        
          
int php_url_decode(char *str, int len)  
{  
        char *dest = str;  
        char *data = str;  
            
        while (len--)   
        {  
            if (*data == '+')   
            {  
                *dest = ' ';  
            }  
            else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1)) && isxdigit((int) *(data + 2)))   
            {  
                *dest = (char) php_htoi(data + 1);  
                data += 2;  
                len -= 2;  
            }   
            else   
            {  
                *dest = *data;  
            }  
            data++;  
            dest++;  
        }  
        *dest = '\0';  // *dest = 0; 用这个好些
        return dest - str;  
}

int main(void)
{
    int len;
    char *lenstr,poststr[200];
    char search_name[100];
            
    memset(poststr, '\0', sizeof(poststr));
    memset(search_name, '\0', sizeof(search_name));
//            printf("Content-Type:text/html\n\n");
    printf("<HTML>\n");
    printf("<HEAD>\n");
    printf("<meta http-equiv=\"Content-Type\" content=\" text/html; charset='utf-8'\" /> ");
    printf(" <TITLE >查询结果</TITLE>\n</HEAD>\n");
            
    printf("<BODY>\n");
    printf("<div style=\"font-size:12px\">\n");
    lenstr=getenv("CONTENT_LENGTH");
    if(lenstr == NULL)
        printf("<DIV STYLE=\"COLOR:RED\">Error parameters should be entered!</DIV>\n");
    else
    {
        len=atoi(lenstr);
        fgets(poststr,len+1,stdin);
        if(sscanf(poststr,"search_name=%[^&]", search_name) != 1)
        {
            printf("<DIV STYLE=\"COLOR:RED\">Error: Parameters are not right!</DIV>\n");    
        }
        else
        {
        
            php_url_decode(search_name, strlen(search_name));
            
            //数据库查找
            char query[1024];
            memset(query, '\0', sizeof(query));
            const char *tmp = "select * from student where name = \"";
            int len = strlen(tmp);
            memcpy(query, tmp, len);
            memcpy(query + len, search_name, strlen(search_name));
            query[len + strlen(search_name)] = '"';

            char *result = (char*)malloc(4096 * sizeof(char));
            if(!result){
                printf("<DIV STYLE=\"COLOR:RED\">Error: Parameters are not right!</DIV>\n");    
                
            }
            else{
                memset(result, '\0', sizeof(char) * 4096);
                query_sql(query, result);
                printf("%s", result);
                free(result);
            }
            //printf("<DIV STYLE=\"COLOR:GREEN; font-size:15px;font-weight:bold\">%s = 美男子 </DIV>\n", search_name);
        }    
    }
    
    printf("<HR COLOR=\"blue\" align=\"left\" width=\"100\">");
    printf("<input type=\"button\" value=\"Back home\" onclick=\"javascript:window.location='../index.html'\">");
    printf("</div>\n");
    printf("</BODY>\n");
    printf("</HTML>\n");
    fflush(stdout);
    return 0;
}

