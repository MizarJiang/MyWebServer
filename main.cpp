#include "./lock/locker.h"
#include "./threadpoll/threadPoll.h"
#include "./http/http_conn.h"

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

using namespace std;

const int MAX_FD = 65535;           // 最大文件描述符数量
const int MAX_EVENT_NUMBER = 10000; // 最大事件数量

// #define connfdET ; // 边缘触发非阻塞
// #define connfdLT ; // 水平触发阻塞

// 设置管道通知函数和epollfd
// 使用静态变量保证其生存周期是整个程序运行阶段
static int pipefd[2];
static int epollfd = 0;

// 为epollfd中添加事件
extern void setNonBlocking(int fd);
extern void addFd(int epollfd, int fd, bool oneShot, std::string temp);

// 信号处理函数，发生信号是通过pipefd发送信息，避免在回调函数中处理浪费时间
void sig_handler(int sig)
{
    int save_error = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    // 试一下使用sendfile零拷贝函数
    // sendfile(pipefd[1], pipefd[0], 0, 1);
    errno = save_error;
}
// 设置信号处理函数
void addSig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        // 在信号处理函数返回时自动重启被中断的系统调用，避免系统调用因为信号而终端
        sa.sa_flags |= SA_RESTART;
    }
    // 确保执行信号处理函数时，不会被其他信号影响
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        cout << "格式错误，调用格式为" << argv[0] << "+端口号" << endl;
        return 1;
    }

    // 当管道关闭时会发送SIGPIPE会导致进程退出
    // 将该信号使用SIG_IGN忽略，保护进程异常退出
    addSig(SIGPIPE, SIG_IGN);

    // 创建线程池
    ThreadPoll<HttpConn> *poll = nullptr;
    try
    {
        poll = new ThreadPoll<HttpConn>();
    }
    catch (...)
    {
        throw std::exception();
        return 1;
    }

    // 创建tcp连接相关操作
    // 创建监听socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd > 0);

    // 绑定
    //  端口号,ip地址使用服务器全部地址
    int port = stoi(argv[1]);
    // 创建addr结构体
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 在bind之前设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 执行绑定与监听
    int bindRet = bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));
    assert(bindRet >= 0);
    int lisRet = listen(listenfd, 10);
    assert(lisRet >= 0);

    // 创建内核时间表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5); // 此参数失效，只要大于0均可
    assert(epollfd > 0);

    addFd(epollfd, listenfd, false, "LT");

    // 创建管道
    int pipeRet = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(pipeRet != -1);
    setNonBlocking(pipefd[1]);
    addFd(epollfd, pipefd[0], false, "LT");
    // sigAlarm
    addSig(SIGALRM, sig_handler, false);
    // ctrl+c
    addSig(SIGTERM, sig_handler, false);
    struct sockaddr_in addr2;
    memset(&addr2, 0, sizeof(addr2));
    socklen_t len = sizeof(addr2);
    int confd = accept(listenfd, (struct sockaddr *)&addr2, &len);
    char buf[1024];
    string str;
    while (1)
    {
        memset(buf, '\0', sizeof(buf));
        int n = recv(confd, buf, sizeof(buf), 0);
        if (n > 0)
        {
            str = buf;
            cout << str << endl;
        }
    }
    return 0;
}