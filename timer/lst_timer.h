#pragma once

#include <time.h>
#include "../log/log.h"
#include <arpa/inet.h>

class util_timer;
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer *prev;
    util_timer *next;
    time_t expire;
    void (*cb_func)(client_data *);
    client_data *user_data;

public:
    util_timer() : prev(nullptr), next(nullptr) {}
};

class sort_timer_list
{
private:
    util_timer *head;
    util_timer *tail;

public:
    sort_timer_list() : head(nullptr), tail(nullptr) {}
    ~sort_timer_list()
    {
        util_timer *temp = head;
        while (temp)
        {
            head = temp->next;
            delete temp;
            temp = head;
        }
    }
    void add_timer(util_timer *timer)
    {
        if (!timer)
            return;
        if (!head)
        {
            head = timer;
            tail = timer;
            return;
        }
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer *temp = timer->next;
        if (!temp || temp->expire > timer->expire)
        {
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            timer->next = nullptr;
            timer->prev = nullptr;
        }
        return;
    }
    void delete_timer(util_timer *timer)
    {
        if (!timer)
            return;
        if (timer == head && timer == tail)
        {
            head = nullptr;
            tail = nullptr;
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
        return;
    }
    void tick()
    {
        if (!head)
            return;
        LOG_INFO("%s", "Timer tick");
        log::GetInstance()->flush();
        time_t cur = time(NULL);
        util_timer *temp = head;
        while (temp)
        {
            if (cur < temp->expire)
            {
                break;
            }
            temp->cb_func(temp->user_data);
            head = temp->next;
            if (head)
            {
                head->prev = nullptr;
            }
            delete temp;
            temp = head;
        }
        return;
    }

private:
    void add_timer(util_timer *time, util_timer *list_head)
    {
    }
};
