#pragma once

#include <iostream>
#include <pthread.h>
#include <queue>

#include "message.h"
#include "conn.h"

class ThreadSafeQueue {
    std::queue<Message> m_queue{};
    mutable pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

public:
    void push(const Message& msg) {
        pthread_mutex_lock(&m_mutex);
        m_queue.push(msg);
        pthread_mutex_unlock(&m_mutex);
    }

    bool pop(Message* msg) {
        pthread_mutex_lock(&m_mutex);
        if (m_queue.empty()) {
        pthread_mutex_unlock(&m_mutex);
            return false;
        }
    
        *msg = m_queue.front();
        m_queue.pop();
    
        pthread_mutex_unlock(&m_mutex);
        return true;
    }

    bool readFrom(Conn* conn) {
        pthread_mutex_lock(&m_mutex);
    
        int cnt{};
        if (!conn->read(&cnt, sizeof(cnt))) {
            pthread_mutex_unlock(&m_mutex);
            return false;
        }
    
        for (int i{}; i < cnt; ++i) {
            Message msg{};
            if (!conn->read(&msg, sizeof(msg))) {
                pthread_mutex_unlock(&m_mutex);
                return false;
            }
            m_queue.push(msg);
        }
    
        pthread_mutex_unlock(&m_mutex);
        return true;
    }

    bool writeTo(Conn* conn) {
        pthread_mutex_lock(&m_mutex);
    
        int cnt = m_queue.size();
        if (!conn->write(&cnt, sizeof(cnt))) {
            pthread_mutex_unlock(&m_mutex);
            return false;
        }
    
        while (!m_queue.empty()) {
            Message msg{ m_queue.front() };
            if (!conn->write(&msg, sizeof(msg))) {
                pthread_mutex_unlock(&m_mutex);
                return false;
            }
            m_queue.pop();
        }
    
        pthread_mutex_unlock(&m_mutex);
        return true;
    }

    void print() {
        pthread_mutex_lock(&m_mutex);

        std::queue<Message> tq = m_queue;
        while (!tq.empty()) {
            std::cout << "\t" << tq.front().text;
            tq.pop();
        }
        
        pthread_mutex_unlock(&m_mutex);
    }
};
