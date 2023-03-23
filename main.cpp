#include "./lock/locker.h"
#include "./threadpool/threadPool.h"
#include "./http/http_conn.h"
#include "./timer/lst_timer.h"
#include "./log/log.h"
#include "./CGImysql/sql_connection_pool.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>

using std::cout;
using std::endl;

const int MAX_FD = 65535;           // 最大文件描述符数量
const int MAX_EVENT_NUMBER = 10000; // 最大事件数量
const int TIMESLOT = 5;             // 最小超时单位

#define SYNLOG // 同步写日志
// #define ASYNLOG //异步写日志

// #define listenfdET //边缘触发非阻塞
#define listenfdLT // 水平触发阻塞
// 在http_conn.cpp中定义
extern int addfd(int epollfd, int fd, bool one_shot);
extern int remove(int epollfd, int fd);
extern int setnonblocking(int fd);

// 设置定时器参数
static int pipefd[2];
static sort_timer_list timer_list;
static int epollfd = 0;

// 信号处理函数
void sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的error
    int save_error = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_error;
}

// 设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        // 在信号处理函数返回时自动重启被中断的系统调用，避免系统调用因为信号而中断
        sa.sa_flags |= SA_RESTART;
    }
    // 确保执行信号处理函数时，不会被其他信号影响
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时来不断触发SIGALRM信号
void timer_handler()
{
    timer_list.tick();
    alarm(TIMESLOT);
}

// 定时器回调函数，删除非活动连接在socket上面的注册事件，关闭文件描述符
void cb_func(client_data *user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    http_conn::m_user_count--;
    LOG_INFO("close fd %d", user_data->sockfd);
    close(user_data->sockfd);
    log::GetInstance()->flush();
}

// 错误打印，关闭错误文件
void error_show(int connfd, const char *info)
{
    cout << info << endl;
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char *argv[])
{
// 同步写
#ifdef SYNLOG
    log::GetInstance()->init("ServerLog", 2000, 800000, 0);
#endif
// 异步写
#ifdef ASYNLOG
    log::GetInstance()->init("ServerLog", 2000, 800000, 8);
#endif
    if (argc < 2)
    {
        cout << "输入格式：" << argv[0] << "+ 端口号" << endl;
        return -1;
    }
    int port = atoi(argv[1]);
    addsig(SIGPIPE, SIG_IGN);
    // 创建数据库连接池
    connection_pool *connPool = connection_pool::GetInstance();
    connPool->init("localhost", "debian-sys-maint", "2QMUiUJuS0mAS0tp", "yourdb", 3306, 8);
    // 创建线程池
    ThreadPoll<http_conn> *pool = NULL;
    try
    {
        pool = new ThreadPoll<http_conn>(connPool);
    }
    catch (...)
    {
        return -1;
    }
    http_conn *users = new http_conn[MAX_FD];
    assert(users >= 0);
    users->initmysql_result(connPool);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // ret作为各种返回值的检查值
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse = 1;
    // 设置端口复用
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 10);
    assert(ret >= 0);

    // 创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    bool stop_server = false;

    client_data *users_timer = new client_data[MAX_FD];

    bool timeout = false;
    alarm(TIMESLOT);
    while (!stop_server)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;
            //
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
#ifdef listenfdLT
                int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (connfd < 0)
                {
                    LOG_ERROR("%s:errno is: %d", "accept error", errno);
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    error_show(connfd, "Server busy");
                    LOG_ERROR("%s", "Server busy");
                    continue;
                }
                users[connfd].init(connfd, client_addr);
                //
                //
                users_timer[connfd].address = client_addr;
                users_timer[connfd].sockfd = connfd;
                util_timer *timer = new util_timer;
                timer->user_data = &users_timer[connfd];
                timer->cb_func = cb_func;
                time_t curTime = time(NULL);
                timer->expire = curTime + 3 * TIMESLOT;
                users_timer[connfd].timer = timer;
                timer_list.add_timer(timer);
#endif

#ifdef listenfdET
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is: %d", "accept error", errno);
                        break;
                    }
                    if (http_conn::m_user_count >= MAX_FD)
                    {
                        error_show(connfd, "Server busy");
                        LOG_ERROR("%s", "Server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_addr);
                    //
                    //
                    users_timer[connfd].address = client_addr;
                    users_timer[connfd].sockfd = connfd;
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t curTime = time(NULL);
                    timer->expire = curTime + 3 * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_list.add_timer(timer);
                }
                continue;
#endif
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //
                util_timer *timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);
                if (timer)
                {
                    timer_list.delete_timer(timer);
                }
            }
            else if ((sockfd == pipefd[0]) && events[i].events & EPOLLIN)
            {
                int sig;
                char signal[1024];
                ret = recv(pipefd[0], signal, sizeof(signal), 0);
                if (ret == -1)
                    continue;
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signal[i])
                        {
                        case SIGALRM:
                        {
                            timeout = true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            //
            else if (events[i].events & EPOLLIN)
            {
                // 获取当前连接的定时器，定时器在listen到时创建成功
                util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].read_once())
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    log::GetInstance()->flush();
                    //
                    pool->append(users + sockfd);

                    if (timer)
                    {
                        time_t curTime = time(NULL);
                        timer->expire = curTime + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        log::GetInstance()->flush();
                        timer_list.adjust_timer(timer);
                    }
                }
                else
                {
                    // read_once失败，关闭连接
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_list.delete_timer(timer);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].write())
                {
                    LOG_INFO("send data to client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    log::GetInstance()->flush();
                    //

                    if (timer)
                    {
                        time_t curTime = time(NULL);
                        timer->expire = curTime + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        log::GetInstance()->flush();
                        timer_list.adjust_timer(timer);
                    }
                }
                else
                {
                    // write失败，关闭连接
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_list.delete_timer(timer);
                    }
                }
            }
        }
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    delete[] users_timer;
    delete pool;
    return 0;
}