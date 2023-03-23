#pragma once

#include <iostream>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template <typename T>
class block_queue
{
private:
    Locker m_mutex;
    Cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;

public:
    block_queue(int max_size = 100)
    {
        if (max_size <= 0)
        {
            cout << "阻塞队列size报错" << endl;
            exit(1);
        }
        m_max_size = max_size;
        m_array = new T[m_max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }
    ~block_queue()
    {
        m_mutex.lock();
        if (m_array != nullptr)
        {
            delete[] m_array;
        }
        m_mutex.unlock();
    }
    void clear()
    {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }
    bool full()
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    bool empty()
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    bool front(T &val)
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        val = m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    bool back(T &val)
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        val = m_array[m_back];
        m_mutex.unlock();
        return true;
    }
    int size()
    {
        int temp = 0;
        m_mutex.lock();
        temp = m_size;
        m_mutex.unlock();
        return temp;
    }
    int max_size()
    {
        int temp = 0;
        m_mutex.lock();
        temp = m_max_size;
        m_mutex.unlock();
        return temp;
    }
    // 往队列添加元素，需要将所有使用队列的线程先唤醒
    // 当有元素push进队列,相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item)
    {
        m_mutex.lock();

        if (m_size >= m_max_size)
        {
            m_cond.broadCast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;

        m_size++;

        m_cond.broadCast();
        m_mutex.unlock();

        return true;
    }
    // pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T &item)
    {
        m_mutex.lock();
        while (m_size <= 0)
        {
            if (!m_cond.wait(m_mutex.getMutex()))
            {
                m_mutex.unlock();
                return false;
            }
        }
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();

        return true;
    }
    // 增加了超时处理
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timeWait(m_mutex.getMutex(), t))
            {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();

        return true;
    }
};