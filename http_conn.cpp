/*************************************************************************
	> File Name: http_conn.cpp
	> Author: 
	> Mail: 
	> Created Time: 2017年02月21日 星期二 12时07分18秒
 ************************************************************************/
#include"http_conn.h"

//定义HTTP响应的一些状态信息
const char * ok_200_title = "OK";
const char *error_400_title = "Bad request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";

//网站根目录
const char *doc_root = "/home/nice/unix/webserver/htdocs";

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}



int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
time_heap* http_conn::m_timer_heap = NULL;

void http_conn::close_conn(bool real_close)
{
    if(real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; //关闭一个连接时，将客户总量建一
        m_timer_heap->del_timer(m_timer);
    }
}

void http_conn::init(int sockfd, const struct sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    //如下两行为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉
   //int reuse = 1;
   // setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    
    //加入定时器
    m_timer = new heap_timer(3 * TIMESLOT);
    if(!m_timer)
    {
        throw std::exception();
    }

    m_timer->sockfd = &m_sockfd;
    m_timer->cb_func = cb_func;

    m_timer_heap->add_timer(m_timer);
    
    init();
}


void http_conn::init()
{
    m_checked_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_read_file, '\0', FILENAME_LEN);
    memset(m_content, '\0', FILENAME_LEN);
}

//从状态机
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;

    for(; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r')
        {
            if((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r'))
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或者对方关闭连接
bool http_conn::read_socket()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;
    
    while(true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            return false;
        }
        
        m_read_idx += bytes_read;
    }

    return true;
}

//解析http请求航，获得请求方法，目标url，已经http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
    }
    else{
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

    m_checked_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    //遇到空行，表示头部字段解析完毕
    if(text[0] == '\0')
    {
        //如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，状态机
        //转移到CHECK_STATE_CONTENT状态
        if(m_content_length != 0)
        {
            m_checked_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经得到了一个完整的HTTP请求
        if(m_method == POST)
            return POST_REQUEST;

        return GET_REQUEST;
    }
    //处理connection头部字段
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    //处理Content-Length头部字段
    else if(strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    //处理Host头部字段
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        printf("oop ! unkown header %s\n", text);
    }

    return NO_REQUEST;
}

//没有真正解析http请求的消息体，只是判断它是否被完整地读入了
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{

    if(m_read_idx >= m_checked_idx)
    {
        text[m_content_length] = '\0';
        if(m_method == POST)
            return POST_REQUEST;
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

//主状态机
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while(((m_checked_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
         || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        printf("got 1 http line: %s\n", text);

        
        switch(m_checked_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                printf("CHECK_STATE_REQUESTLINE\n");
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER:
            {
                printf("CHECK_STATE_HEADER\n");
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST)
                {
                    
                    return do_get_request();
                }
                else if(ret == POST_REQUEST)
                {
                    return do_post_request();
                }
                break;
            }

            case CHECK_STATE_CONTENT:
            {
                printf("CHECK_STATE_CONTENT\n");
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                {
                    return do_get_request();
                }
                else if(ret == POST_REQUEST)
                {
                    return do_post_request();
                }

                line_status = LINE_OPEN;
                break;
            }
            
            default:
            {
                printf("default\n");
                return INTERNAL_ERROR;
            }
        }
    }
    
    return NO_REQUEST;
}

//当得到一个完整正确的HTTP请求时，我们就分析目标文件的属性，如果目标文件存在，
//对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，
//并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_get_request()
{
    strcpy(m_read_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_read_file + len, m_url, FILENAME_LEN - len -1);
    if(m_read_file[strlen(m_read_file) - 1] == '/')
    {
        strcat(m_read_file, "index.html");
    }

    if(stat(m_read_file, &m_file_stat) < 0)
    {
        return NO_REQUEST;
    }

    if(!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    if(S_ISDIR(m_file_stat.st_mode))
    {
        strcat(m_read_file, "/index.html");
       // return BAD_REQUEST;
    }

    printf("path: %s\n", m_read_file);
    int fd = open(m_read_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ,
                                MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

//对内存映射区执行munmap操作
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}


http_conn::HTTP_CODE http_conn::do_post_request()
{
    int status;
    char path[FILENAME_LEN];
    memset(path, '\0', sizeof(path));

    strcpy(path, doc_root);

    int len = strlen(doc_root);
    strncpy(path + len, m_url, FILENAME_LEN - len - 1);
    
    struct stat file_stat; 

    if(stat(path, &file_stat) < 0)
    {
        return NO_REQUEST;
    }

    if(!(file_stat.st_mode & S_IROTH))
    {
        return NO_REQUEST;
    }
    

    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;

    printf("=============================================\n");
    //建立管道,两个通道，cgi_output[0]读端，cgi_output[1]写端
    if(pipe(cgi_output) < 0)
    {
        printf("pipe error.\n");
        return BAD_REQUEST;
    }

    if(pipe(cgi_input) < 0)
    {
        printf("pipe error.\n");
        return BAD_REQUEST;
    }

    //fork子进程，这就创建了父子间通信的IPC通道
    if((pid = fork()) < 0)
    {
        printf("fork error.\n");
        return BAD_REQUEST;
    }

    //实现初始化两个管道通信机制
    //子进程继承了父进程的pipe
    if(pid == 0)
    {
        //设置环境变量
        char meth_env[255];
        char length_env[255];

        //复制文件描述符，重定向进程的标准输入和输出
        dup2(cgi_output[1], STDOUT);  //标准输出重定向到output管道的写入端
        dup2(cgi_input[0], STDIN);  //标准输入重定向到input的读取端
        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(meth_env, "REQUEST_METHOD=POST");
        putenv(meth_env);  

        sprintf(length_env, "CONTENT_LENGTH=%d", m_content_length);
        putenv(length_env);

        execl(path, (char*)0); 
        exit(0);
    }
    //父进程
    else
    {
        close(cgi_output[1]);
        close(cgi_input[0]);
        //通过关闭对应管道的通道，然后重定向子进程的管道某端，这样就在父子进程之间建立了一条单双工
        //通道，如果不重定向，将是一条典型的全双工管道机制
        //


        for(int i = 0; i < m_content_length; ++i)
        {
            printf("lalalal=%c\n", m_read_buf[m_checked_idx]);
            write(cgi_input[1], &m_read_buf[m_checked_idx++], 1);
            
        }
        

       const char* filename = "/result.html";
        strcpy(m_read_file, doc_root);
        int len = strlen(doc_root);
        strncpy(m_read_file + len, filename, FILENAME_LEN - len -1);

        int fd = open(m_read_file, O_RDWR | O_CREAT | O_TRUNC);
        assert(fd != -1);

        printf("====================================\n");
        char c;
        while(read(cgi_output[0], &c, 1) > 0)
        {
            printf("%c", c);
            write(fd, &c, 1);
        }
        
        close(fd);
        
        fd = -1;
        fd = open(m_read_file, O_RDONLY);
        assert(fd != -1);
        if(stat(m_read_file, &m_file_stat) < 0)
        {
            return NO_REQUEST;
        }

        m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ,
                             MAP_PRIVATE, fd, 0);
        close(fd);    
        waitpid(pid, &status, 0);
    }

    return FILE_REQUEST;
}

//写http响应
bool http_conn::write_socket()
{
    printf("write 响应.\n");
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    printf("m_write_idx %d %s\n", m_write_idx, m_write_buf);
    if(bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    bool flag = false;
    while(1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1)
        {
            //如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            //服务器无法立即接收到同一个客户的下一个请求，但这可以保证连接的完整性
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            break;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        printf("to_send %d  has_send %d temp %d\n",bytes_to_send, bytes_have_send, temp);
        if(bytes_to_send <= bytes_have_send)
        {
            //发送HTTP响应成功，根据http请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger)
            {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                flag = true;
                break;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                break;
            }
        }
    }
    printf("write end.\n");
    return flag;
}

//往写缓冲中写入待发送的数据
bool http_conn::add_response(const char* format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,
                       format, arg_list);
    
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        return false;
    }

    m_write_idx += len;
    va_end(arg_list);
    return true;
}


bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s", content);
}


//根据服务器处理http请求的结果，决定返回给客户端的内容
bool http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
            {
                return false;
            }
            break;
        }

        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form))
            {
                return false;
            }

            break;
        }

        case NO_REQUEST:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
            {
                return false;
            }
            break;
        }

        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
            {
                return false;
            }
            break;
        }

        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0)
            {
                
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else{
                const char * ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                {
                    return false;
                }
            }
            break;
        }
        
        default:
        {
                return false;
        }
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}


//由线程池的工作线程调用，这是处理http请求的入口函数
void http_conn::process()
{
    printf("first.\n");
    HTTP_CODE read_ret = process_read();

    printf("hello: process() 出口  \n");
    if(read_ret == NO_REQUEST)
    {
        printf("read_ret NO_REQUEST exit\n");
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }
    
    if(read_ret == FILE_REQUEST) 
        printf("hahah process_write in. %d\n", read_ret);
    bool write_ret = process_write(read_ret);

    printf("process_write out. %d\n", write_ret ? 1 : 0);
    if(!write_ret)
    {
        close_conn();
    }

    modfd(m_epollfd, m_sockfd, EPOLLOUT);

    printf("ok?\n");
}
