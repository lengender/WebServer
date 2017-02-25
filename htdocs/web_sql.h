/*************************************************************************
	> File Name: mysql.h
	> Author: 
	> Mail: 
	> Created Time: 2017年02月24日 星期五 16时44分52秒
 ************************************************************************/

#ifndef _WEB_SQL_H
#define _WEB_SQL_H

#include<mysql.h>
#include<stdio.h>
#include<string.h>


void query_sql(const char *sql, char *result);
void insert_sql(const char *sql);

#endif
