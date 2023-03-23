#include "log.h"
#include <string.h>
#include <pthread.h>
#include <stdarg.h>

using namespace std;

log::log()
{
    m_log_count = 0;
    m_is_async = false;
}

log::~log()
{
    if (m_fp != nullptr)
        fclose(m_fp);
}
// 异步需要设置阻塞队列长度，同步不需要设置
bool log::init(const char *file_name, int log_buf_size, int split_lines,
               int max_queue_size)
{
    // 如果设置了max_queue_size，则使用异步
    if (max_queue_size > 0)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t pid;
        // flush_log_thread作为回调函数，这里表示创建线程异步写日志
        pthread_create(&pid, nullptr, flush_log_thread, nullptr);
    }
    // 输出内容的长度
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    // 最大行数
    m_split_lines = split_lines;

    // 获取当前时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    // strrchr()在一个字符串中查找最后一个指定字符的位置
    // 从后往前找到第一个/的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // p==NULL说明输入参数中不包含路径
    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    // p！=NULL说明输入参数中包含路径，此时p被指向'/'
    else
    {
        // 将/的位置向后移动一个位置，然后复制到logname中
        // p - file_name + 1是文件所在路径文件夹的长度
        // dirname相当于./
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;
    //"a"表示以追加方式打开文件，如果文件不存在则会创建一个新文件。
    m_fp = fopen(log_full_name, "a");
    if (m_fp == nullptr)
        return false;
    return true;
}
// 执行写日志动作
void log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]");
        break;
    case 1:
        strcpy(s, "[info]");
        break;
    case 2:
        strcpy(s, "[warn]");
        break;
    case 3:
        strcpy(s, "[error]");
        break;
    default:
        strcpy(s, "[info]");
        break;
    }
    // 写入一个log，对m_log_count++,m_split_lines
    m_mutex.lock();
    m_log_count++;
    if (m_today != my_tm.tm_mday || m_log_count % m_split_lines == 0)
    {
        // 当写入日志的行数除以设置的最大行数的榆树为0时，需要切换到新的日志文件
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_log_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_log_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    va_list valist;
    va_start(valist, format);

    string log_str;
    m_mutex.lock();

    // 写入时间的具体格式,n为输出的字符数
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // bug
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valist);
    // int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valist);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valist);
}
// 刷新缓冲区
void log::flush(void)
{
    m_mutex.lock();
    // 将m_fp从缓冲区刷新进磁盘并清空缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}