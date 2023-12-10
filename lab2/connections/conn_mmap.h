#pragma once

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "conn.h"

class ConnMMAP : public Conn {
    std::string m_filepath{};
    pid_t m_pid{};
    bool m_isCreate{};
    int m_fd{ -1 };
    
    int m_shift{};
    
    void* m_host_buf{};
    int m_host_shift{};

    void* m_client_buf{};
    int m_client_shift{};

public:
    ConnMMAP(pid_t pid, bool isCreate) {
        m_isCreate = isCreate; 
    }
    ~ConnMMAP() {
        munmap(m_host_buf, MAX_SIZE);
        munmap(m_client_buf, MAX_SIZE);
    }

    bool open(pid_t pid, bool isCreate) override {
        if (!isCreate) return true;

        m_host_buf = mmap(nullptr, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (m_host_buf == MAP_FAILED) {
            syslog(LOG_ERR, "mmap(2) call error for host buf: %s", strerror(errno));
            return false;
        }

        m_client_buf = mmap(nullptr, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (m_client_buf == MAP_FAILED) {
            syslog(LOG_ERR, "mmap(2) call error for host buf: %s", strerror(errno));
            munmap(m_host_buf, MAX_SIZE);
            return false;
        }

        return true;
    }

    bool read(void* buf, size_t size) override {
        if (m_isCreate) {
            if (size + m_host_shift >= MAX_SIZE)
                m_host_shift = 0;
            memcpy(buf, (char*)m_host_buf + m_host_shift, size);
            m_host_shift += size;
        }
        else {
            if (size + m_client_shift >= MAX_SIZE)
                m_client_shift = 0;
            memcpy(buf, (char*)m_client_buf + m_client_shift, size);
            m_client_shift += size;
        }
        return true;
    }
    bool write(void* buf, size_t size) override {
        if (m_isCreate) {
            if (size + m_host_shift >= MAX_SIZE)
                m_host_shift = 0;
            memcpy((char*)m_host_buf + m_host_shift, buf, size);
            m_host_shift += size;
        }
        else {
            if (size + m_client_shift >= MAX_SIZE)
                m_client_shift = 0;
            memcpy((char*)m_client_buf + m_client_shift, buf, size);
            m_client_shift += size;
        }
        return true;
    }
};
