/*************************************************************************
	> File Name: webserver.h
	> Author: 
	> Mail: 
	> Created Time: 2017年02月23日 星期四 15时15分33秒
 ************************************************************************/

#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include<sys/socket.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<arpa/inet.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>
#include"threadpool.h"
#include"locker.h"
#include"http_conn.h"
#include"min_heap_timer.h"


const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

void addsig(int sig, void (handler)(int), bool restart);
void show_error(int connfd, const char* info);
void timer_handler(time_heap *heap);
void cb_func(int *sockfd);
void sig_handler(int sig);


#endif
