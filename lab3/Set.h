#pragma once

#include <pthread.h>
#include <string>
#include <sys/syslog.h>
#include <stdexcept>
#include <mutex>

template <typename T>
class Set {
    struct Node {
        T m_data;
        Node* m_next{};
        Node* m_rmNext{};
        pthread_mutex_t m_mutex;
    
        Node() = delete;
        Node(int &res) : m_data() {
            res = pthread_mutex_init(&m_mutex, 0);
            if (res) syslog(LOG_ERR, "Failed to init mutex, code %d", res);
        }
        Node(int &res, const T& data, Node* next = nullptr) : m_data(data), m_next(next) {
            res = pthread_mutex_init(&m_mutex, 0);
            if (res) syslog(LOG_ERR, "Failed to init mutex, code %d", res);
        }

        ~Node() {
            int res{ pthread_mutex_destroy(&m_mutex) };
            if (res) syslog(LOG_ERR, "Failed to destroy mutex, code %d", res);
        }
    
        inline void lock() {
            int res{ pthread_mutex_lock(&m_mutex) };
            if (res) throw std::runtime_error("Failed to lock mutex, code " + std::to_string(res));
        }
    
        inline void unlock() {
            int res{ pthread_mutex_unlock(&m_mutex) };
            if (res) syslog(LOG_ERR, "Failed to unlock mutex, code %d", res);
        }
    };

    Node* m_head;
    Node* m_rmHead;
    int (*m_cmp)(const T &a, const T &b);

public:
    Set(int &res, int (*cmp)(const T &a, const T &b)) : m_cmp(cmp) {
        m_head = new Node(res = 0);
        if (res) {
            syslog(LOG_ERR, "Failed to init node, code %d", res);
            delete m_head;
            return;
        }

        m_rmHead = new Node(res);
        if (res) {
            syslog(LOG_ERR, "Failed to init node, code %d", res);
            delete m_head;
            delete m_rmHead;
            return;
        }
    }

    ~Set() {
        Node* curr{ m_rmHead->m_rmNext };
        m_rmHead->m_rmNext = nullptr;
        while (curr) {
            Node* tmp{ curr };
            curr = curr->m_rmNext;
            delete tmp;
        }
        delete m_rmHead;

        curr = m_head->m_next;
        while (curr) {
            Node* tmp{ curr };
            curr = curr->m_next;
            delete tmp;
        }
        delete m_head;
    }

    void move(const T &data, Node** prev, Node** curr) {
        (*prev) = m_head;
        (*curr) = m_head->m_next;
        while ((*curr) && m_cmp((*curr)->m_data, data) < 0) {
            (*prev) = (*curr);
            (*curr) = (*curr)->m_next;
        }
    }

    bool add(const T& data) {
        Node* prev{}, *curr{};
        move(data, &prev, &curr);
        
        prev->lock();
        if (curr) curr->lock();
        if (!isValid(prev, curr)) {
            prev->unlock();
            if (curr) curr->unlock();
            return add(data);
        }
        
        if (curr && !m_cmp(curr->m_data, data)) return false;
                
        int res{};
        Node *node{new Node(res, data, curr) };
        if (res) {
            delete m_head;
            throw std::runtime_error("Failed to init node, code " + std::to_string(res));
        }

        prev->m_next = node;

        prev->unlock();
        if (curr) curr->unlock();
        return true;
    }

    bool remove(const T &data) {
        Node* prev{}, *curr{};
        move(data, &prev, &curr);
        if (!curr) return false;

        prev->lock();
        curr->lock();
        if (!isValid(prev, curr)) {
            prev->unlock();
            curr->unlock();
            return remove(data);
        }

        if (!m_cmp(curr->m_data, data)) {
            prev->m_next = curr->m_next;
            prev->unlock();
            curr->unlock();
            std::unique_lock<Node> predLock(*m_rmHead);
            curr->m_rmNext = m_rmHead->m_rmNext;
            m_rmHead->m_rmNext = curr;
            return true;
        }
        prev->unlock();
        curr->unlock();
        return false;
    }

    bool contains(const T &data) {
        Node* prev{}, *curr{};
        move(data, &prev, &curr);
        if (!curr) return false;

        prev->lock();
        curr->lock();
        if (!isValid(prev, curr)) {
            prev->unlock();
            curr->unlock();
            return contains(data);
        }
        prev->unlock();
        curr->unlock();
        return !m_cmp(curr->m_data, data);
    }

private:
    bool isValid(Node* prev, Node* curr) {
        Node *node{ m_head };
        while (node && m_cmp(node->m_data, prev->m_data) <= 0) {
            if (node == prev) return prev->m_next == curr;
            node = node->m_next;
        }
        return false;
    }
};