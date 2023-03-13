#pragma once
// 互斥锁
#include <pthread.h>
// 信号量
#include <semaphore.h>
// 异常报错
#include <exception>

class Locker
{
private:
    pthread_mutex_t m_mutex;

public:
    Locker()
    {
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            throw std::exception();
        }
    }
    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    // 上锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    // 解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    // 返回锁接口
    pthread_mutex_t *getMutex()
    {
        return &m_mutex;
    }
};

class Sem
{
private:
    sem_t m_sem;

public:
    Sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
            throw std::exception();
    }
    Sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
            throw std::exception();
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
};

class Cond
{
private:
    pthread_cond_t m_cond;

public:
    Cond()
    {
        if (pthread_cond_init(&m_cond, nullptr) != 0)
            throw std::exception();
    }
    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        int res = 0;
        // pthread_mutex_lock(m_mutex);
        res = pthread_cond_wait(&m_cond, m_mutex);
        // pthread_mutex_unlock(m_mutex);
        return res == 0;
    }
    bool timeWait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int res = 0;
        // pthread_mutex_lock(m_mutex);
        res = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        // pthread_mutex_unlock(m_mutex);
        return res == 0;
    }
    bool post()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadCast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};
