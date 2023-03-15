#pragma once

#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include "../lock/locker.h"

using namespace std;

class connection_poll
{
private:
    connection_poll();
    ~connection_poll();

    unsigned int MaxConn;  // 最大连接数
    unsigned int CurConn;  // 当前已使用的连接数
    unsigned int FreeConn; // 当前空闲的连接数

    Locker lock;
    list<MYSQL *> connList; // 连接池
    Sem reserve;

private:
    string url;          // 主机地址
    string Port;         // 数据库端口号
    string User;         // 登录数据库用户名
    string PassWord;     // 登录数据库密码
    string DatabaseName; // 使用数据库名称

public:
    MYSQL *GetConnection();              // 获取数据库连接
    bool ReleaseConnection(MYSQL *conn); // 释放连接
    int GetFreeConn();                   // 获取连接
    void DestroyPool();                  // 销毁连接

    // 单例饿汉模式
    static connection_poll *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName,
              int Prot, unsigned int MaxConn);
};
// 自动化管理连接的获取和释放
class connectionRAII
{
private:
    MYSQL *conRAII;
    connection_poll *pollRAII;

public:
    // 获取连接
    connectionRAII(MYSQL **con, connection_poll *connPoll);
    // 释放连接
    ~connectionRAII();
};