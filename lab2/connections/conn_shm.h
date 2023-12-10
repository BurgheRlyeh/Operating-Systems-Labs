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

class ConnSHM : public Conn {
    std::string m_filepath{};
    pid_t m_pid{};
    bool m_isCreate{};
    int m_fd{ -1 };
    void* m_buf{};
    int m_shift{};

public:
    ConnSHM(pid_t pid, bool isCreate) {
        m_pid = pid;
        m_filepath = "shm_" + std::to_string(pid);
        m_isCreate = isCreate;
    }
    ~ConnSHM() {
        munmap(m_buf, MAX_SIZE);
        close(m_fd);
        if (m_isCreate)
            shm_unlink(m_filepath.c_str());
    }

    bool open(pid_t pid, bool isCreate) override {
        int flag{ m_isCreate ? O_CREAT | O_RDWR | O_EXCL : O_RDWR };
        if ((m_fd = shm_open(m_filepath.c_str(), flag, 0666)) == -1) {
            syslog(LOG_ERR, "shm_open(3) call error: %s", strerror(errno));
            return false;
        }

        if (ftruncate(m_fd, MAX_SIZE) == -1) {
            syslog(LOG_ERR, "ftruncate(2) call error: %s", strerror(errno));
            return false;
        }

        m_buf = mmap(0, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, m_fd, 0);
        if (m_buf == MAP_FAILED) {
            syslog(LOG_ERR, "mmap(2) call error: %s", strerror(errno));
            close(m_fd);
            if (isCreate)
                shm_unlink(m_filepath.c_str());
            return false;
        }

        return true;
    }

    bool read(void* buf, size_t size) override {
        if (!m_buf || !buf) return false;
        if (size + m_shift >= MAX_SIZE) m_shift = 0;
        memcpy(buf, (char*)m_buf + m_shift, size);
        m_shift += size;
        return true;
    }
    bool write(void* buf, size_t size) override {
        if (!m_buf || !buf) return false;
        if (size + m_shift >= MAX_SIZE) m_shift = 0;
        memcpy((char*)m_buf + m_shift, buf, size);
        m_shift += size;
        return true;
    }
};
