#pragma once

#include "block_queue.h"
#include <pthread.h>
#include <string>
#include <iostream>
#include <stdarg.h>

using namespace std;

class log
{
private:
    char dir_name[128];    // 路径名
    char log_name[128];    // log文件名
    int m_split_lines;     // 日志最大行数
    int m_log_buf_size;    // 日志缓冲区大小
    long long m_log_count; // 日志行数记录
    int m_today;           // 记录以天为单位
    FILE *m_fp;            // 打开log文件的指针
    char *m_buf;
    block_queue<string> *m_log_queue; // 阻塞队列
    bool m_is_async;                  // 是否异步
    Locker m_mutex;

private:
    log();
    virtual ~log();
    void *async_write_log()
    {
        string sigle_log;
        while (m_log_queue->pop(sigle_log))
        {
            m_mutex.lock();
            fputs(sigle_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

public:
    static log *GetInstance()
    {
        static log instance;
        return &instance;
    }
    static void *flush_log_thread(void *arg)
    {
        log::GetInstance()->async_write_log();
    }
    // 初始化定义日志文件，日志缓冲区大小，最大行数，最长日志队列
    bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000,
              int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);
};

#define LOG_DEBUG(format, ...) log::GetInstance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) log::GetInstance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) log::GetInstance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log::GetInstance()->write_log(3, format, ##__VA_ARGS__)