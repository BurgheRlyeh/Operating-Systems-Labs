#pragma once

#include <errno.h>
#include <fcntl.h>
#include <string>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conn.h"

class ConnFIFO : public Conn {
    std::string m_filepath{};
    bool m_isCreate{};
    int m_fd{ -1 };

public:
    ConnFIFO(pid_t pid, bool isCreate) {
        m_filepath = "/tmp/fifo_" + std::to_string(pid);
        m_isCreate = isCreate;
    }
    ~ConnFIFO() {
        close(m_fd);
        if (m_isCreate) 
            unlink(m_filepath.c_str());
    }

    bool open(pid_t pid, bool isCreate) override {
        if (m_isCreate) {
            unlink(m_filepath.c_str());
            if (mkfifo(m_filepath.c_str(), 0666)) {
                syslog(LOG_ERR, "mkfifo(3) call error: %s", strerror(errno));
                return false;    
            }
            syslog(LOG_INFO, "fifo created");
        }

        if ((m_fd = ::open(m_filepath.c_str(), O_RDWR)) == -1) {
            syslog(LOG_ERR, "open(2) call error: %s", strerror(errno));
            return false;    
        }
        syslog(LOG_INFO, "fifo opened");
        return true;
    }

    bool read(void* buf, size_t size) override {
        if (::read(m_fd, buf, size) == -1) {
            syslog(LOG_ERR, "read(3) call error: %s", strerror(errno));
            return false;
        }
        return true;
    }
    bool write(void* buf, size_t size) override {
        if (::write(m_fd, buf, size) == -1) {
            syslog(LOG_ERR, "write(3) call error: %s", strerror(errno));
            return false;
        }
        return true;
    }
};
