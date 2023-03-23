#pragma once

#include <list>
#include <exception>
#include <iostream>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class ThreadPoll
{
private:
    // 线程工作的函数，为了多线程函数能够调用，使用静态函数
    static void *worker(void *arg);
    // 实际上调用的函数
    void run();

private:
    int m_thread_num;            // 线程池中的线程数量
    int m_max_request;           // 请求队列中允许的最大请求数量
    pthread_t *m_threads;        // 请求队列的数组，长度为m_thread_num
    std::list<T *> m_workqueue;  // 请求队列
    Locker m_queueLocker;        // 请求队列互斥锁
    Sem m_queueSem;              // 请求队列互斥量
    bool m_stop;                 // 是否结束线程
    connection_pool *m_connpoll; // 数据库
public:
    ThreadPoll(connection_pool *connPoll, int thread_num = 8, int max_request = 10000);
    ~ThreadPoll();
    bool append(T *request);
};
/*
类模板中，类模板的构造函数在类模板外部定义时不能带有默认参数。
需要将 ThreadPoll 构造函数的默认参数移到类模板定义的部分
*/
template <typename T>
ThreadPoll<T>::ThreadPoll(connection_pool *connPoll, int thread_num, int max_request)
    : m_thread_num(thread_num), m_max_request(max_request), m_stop(false), m_threads(nullptr), m_connpoll(connPoll)
{
    if (m_thread_num <= 0 || m_max_request <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < m_thread_num; i++)
    {
        std::cout << "创建线程" << i + 1 << std::endl;
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPoll<T>::~ThreadPoll()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
void *ThreadPoll<T>::worker(void *arg)
{
    ThreadPoll *poll = (ThreadPoll *)arg;
    poll->run();
    return poll;
}

template <typename T>
bool ThreadPoll<T>::append(T *request)
{
    m_queueLocker.lock();
    if (m_workqueue.size() > m_max_request)
    {
        m_queueLocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queueLocker.unlock();
    m_queueSem.post();
    return true;
}

template <typename T>
void ThreadPoll<T>::run()
{
    // bug
    while (!m_stop)
    // while (true)
    {
        m_queueSem.wait();
        m_queueLocker.lock();
        if (m_workqueue.empty())
        {
            m_queueLocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queueLocker.unlock();
        if (!request)
        {
            std::cout << "空请求" << std::endl;
            continue;
        }
        connectionRAII mysqlcon(&request->mysql, m_connpoll);
        // 执行业务逻辑
        request->process();
        // delete request;
    }
    return;
}