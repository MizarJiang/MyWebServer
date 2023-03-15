#include "sql_connection_pool.h"
#include <pthread.h>
#include <iostream>
#include <list>
#include <string>
#include <string.h>

using namespace std;

connection_pool::connection_pool()
{
    this->CurConn = 0;
    this->FreeConn = 0;
}
// 使用静态变量实现懒汉
connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

// 构造初始化
void connection_pool::init(string url, string User, string PassWord, string DatabaseName,
                           int Port, unsigned int MaxConn)
{
    // 非构造函数不能使用列表初始化
    // 参数初始化
    this->url = url;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DatabaseName;
    this->Port = Port;
    this->MaxConn = MaxConn;
    // 上锁
    lock.lock();
    // 循环创建一定数量的数据库连接，将连接添加到连接池的列表中，将空闲连接数增加
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = nullptr;
        con = mysql_init(con);
        if (con == nullptr)
        {
            cout << "Error: " << mysql_errno(con) << endl;
            exit(1);
        }
        // 将参数连接成功的MYSQL句柄赋值给con
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),
                                 DatabaseName.c_str(), Port, nullptr, 0);

        if (con == nullptr)
        {
            cout << "Error: " << mysql_errno(con) << endl;
            exit(1);
        }
        connList.push_back(con);
        FreeConn++;
    }
    // 声明信号量数量，表示有多少个连接可以被获取
    reserve = Sem(FreeConn);
    // 更新最大连接数
    this->MaxConn = FreeConn;
    lock.unlock();
}
// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = nullptr;
    if (connList.size() == 0)
        return nullptr;
    // 信号量减一
    reserve.wait();

    lock.lock();

    con = connList.front();
    connList.pop_front();

    FreeConn--;
    CurConn++;
    lock.unlock();
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if (con == nullptr)
        return false;
    lock.lock();

    connList.push_back(con);
    FreeConn++;
    CurConn--;

    lock.unlock();
    reserve.post();
    return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); it++)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

// 获取当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *conn_pool)
{
    *SQL = conn_pool->GetConnection();
    conRAII = *SQL;
    pollRAII = conn_pool;
}

connectionRAII::~connectionRAII()
{
    pollRAII->ReleaseConnection(conRAII);
}