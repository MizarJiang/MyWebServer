#include "http_conn.h"
#include <iostream>
#include <sys/epoll.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
// 对文件描述符设置非阻塞
int setNonBlocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
// 将内核事件表注册读事件，temp选择开启ET/LT模式，选择开启EPOLLONESHOT,
void addFd(int epollfd, int fd, bool oneShot, std::string temp)
{
    epoll_event event;
    event.data.fd = fd;
    if (temp == "ET")
    {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }
    else if (temp == "LT")
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    else
    {
        std::cout << "输入参数错误" << std::endl;
        exit(1);
    }
    if (oneShot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}
// 从内核事件中移除文件描述符
void removeFd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void HttpConn::print()
{
    std::cout << "执行业务" << std::endl;
}

void HttpConn::process()
{
    print();
    return;
}