/*************************************************************************
	> File Name: webserver.cpp
	> Author: 
	> Mail: 
	> Created Time: 2017年02月21日 星期二 20时12分11秒
 ************************************************************************/

#include"webserver.h"

static int pipefd[2];

void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART; 
    }

    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void timer_handler(time_heap *tHeap)
{
    //定时处理任务，实际上就是调用tick()函数
    tHeap->tick();

    //因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发
    //SIGALRM信号
    if(!tHeap->empty())
    {
        heap_timer* tmp = tHeap->top();
        alarm(tmp->expire - time(NULL));
    }
    else{
        alarm(TIMESLOT);
    }
}

void cb_func(int *sockfd)
{
    assert(sockfd);
    
    if(*sockfd != -1)
    {
        removefd(http_conn::m_epollfd, *sockfd);
        *sockfd = -1;
        http_conn::m_user_count--;
    }
}


void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}



int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    //忽略SIGPIPE信号
    addsig(SIGPIPE, SIG_IGN);

    printf("before new threadpool.\n");
    //创建线程池
   threadpool<http_conn> *pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }
    catch(...)
    {
        return 1;
    }
    
    printf("after new threadpool.\n");
    //预先为每个可能的客户连接分配一个http_conn对象
    http_conn *users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;

    time_heap *heap = new time_heap(8);
    if(!heap)
    {
        throw std::exception();
    }
    http_conn::m_timer_heap = heap;
    
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = {1 , 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);

    //设置信号处理函数
    addsig(SIGALRM, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);

    bool stop_server = false;
    bool timeout = false;
    alarm(TIMESLOT);

    while(!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            printf("epoll failure.\n");
            break;
        }

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_addrlength);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address,
                                   &client_addrlength);

                if(connfd < 0)
                {
                    printf("errno is : %d\n", errno);
                    continue;
                }

                if(http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                
                printf("user connect: %d\n", connfd);
                //初始化客户连接
                users[connfd].init(connfd, client_address);
            }
            else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                memset(signals, '\0', sizeof(signals));

                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1)
                {
                    //handler to error;
                    printf("pipefd[0] recv error.\n");
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                else
                {
                    for(int i = 0; i < ret; ++i)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:
                            {
                                //用timeout变量标记有定时任务需要处理，但不立即处理定时任务，
                                //这是因为定时任务的优先级不是很高，我们优先处理其他更重要
                                //的任务
                                timeout = true;
                                break;
                            }
                            
                            case SIGINT:
                            case SIGTERM:
                            {
                                stop_server = true;
                                break;
                            }
                        }
                    }
                }
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //如果有异常，直接关闭客户连接
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN)
            {
                //根据读的结果，决定是将任务添加到线程池，还是关闭连接
                if(users[sockfd].read_socket())
                {
                    //增加定时值
                    heap_timer *timer = users[sockfd].m_timer;
                    if(timer)
                    {
                        users[sockfd].m_timer_heap->adjust_timer(timer, 3 * TIMESLOT);
                    }
                    pool->append(users + sockfd);

                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                //根据写的结果，决定是否关闭连接
                if(!users[sockfd].write_socket())
                {
                    users[sockfd].close_conn();
                }
            }
            else
            {}
        }

        if(timeout)
        {
            timer_handler(http_conn::m_timer_heap);
            timeout = false;
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    delete heap;

    return 0;

}
