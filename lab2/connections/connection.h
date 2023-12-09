#pragma once

#include <atomic>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <semaphore.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>

#include "tsqueue.h"
#include "conn.h"

class Connection {
    Conn* conn{};
    sem_t* sem_read{};
    sem_t* sem_write{};
    
    ThreadSafeQueue inMsgs{};
    ThreadSafeQueue outMsgs{};

    std::chrono::time_point<std::chrono::high_resolution_clock> lastMsgTime;

public:
    std::chrono::time_point<std::chrono::high_resolution_clock> getLastMsgTime() {
        return lastMsgTime;
    }

    bool open(pid_t pidClient, pid_t pidHost, bool isHost) {
        conn = Conn::create(pidClient, isHost);
        if (!conn) return false;

        std::string sem_read_name{ "/host_" + std::to_string(pidClient) };
        std::string sem_write_name{ "/client_" + std::to_string(pidClient) };
        if (!isHost) std::swap(sem_read_name, sem_write_name);

        if (!isHost) sem_read = sem_open(sem_read_name.c_str(), 0);
        else sem_read = sem_open(sem_read_name.c_str(), O_CREAT | O_EXCL, 0777, 0);
        if (sem_read == SEM_FAILED) {
            syslog(LOG_ERR, "sem_open(3) call error for read sem: %s", strerror(errno));
            delete conn;
            return false;
        }

        if (!isHost) sem_write = sem_open(sem_write_name.c_str(), 0);
        else sem_write = sem_open(sem_write_name.c_str(), O_CREAT | O_EXCL, 0777, 0);
        if (sem_write == SEM_FAILED) {
            syslog(LOG_ERR, "sem_open(3) call error for write sem: %s", strerror(errno));
            sem_close(sem_read);
            delete conn;
            return false;
        }

        if (isHost && kill(pidClient, SIGUSR1) != 0) {
            syslog(LOG_ERR, "kill(2) call error: %s", strerror(errno));
            sem_close(sem_write);
            sem_close(sem_read);
            delete conn;
            return false;
        }

        if (!conn->open(pidHost, isHost)) {
            sem_close(sem_write);
            sem_close(sem_read);
            delete conn;
            return false;
        }

        return true;
    }

    void close() {
        sem_close(sem_write);
        sem_close(sem_read);
        if (conn) delete conn;
    }

    static void* input_loop(void* c) {
        return reinterpret_cast<Connection*>(c)->nonstatic_input_loop();
    }

    static void* host_loop(void* c) {
        return reinterpret_cast<Connection*>(c)->nonstatic_host_loop();
    }

    static void* client_loop(void* c) {
        return reinterpret_cast<Connection*>(c)->nonstatic_client_loop();
    }

private:
    bool read(const std::string& username) {
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_sec += 5;

        if (sem_timedwait(sem_read, &t) == -1) {
            syslog(LOG_ERR, "sem_timedwait call error: %s", strerror(errno));
            return false;
        }
        
        inMsgs.readFrom(conn);
        Message msg;
        if (inMsgs.pop(&msg)) {
            lastMsgTime = std::chrono::high_resolution_clock::now();
            std::cout << username << ": " << msg.text << std::endl;       
        }
        return true;
    }

    bool write() {
        bool res{ outMsgs.writeTo(conn) };
        sem_post(sem_write);
        return res;
    }

    void* nonstatic_input_loop() {
        for(;;) {
            if (std::cin.peek() == EOF) continue;

            std::string text{};
            std::cin >> std::ws;
            std::getline(std::cin, text);
            Message msg{};
            strncpy(msg.text, text.c_str(), text.size());
            outMsgs.push(msg);
        }
        return nullptr;
    }

    void* nonstatic_host_loop() {
        lastMsgTime = std::chrono::high_resolution_clock::now();
        while (read("Client") && write());
        return nullptr;
    }

    void* nonstatic_client_loop() {
        while (write() && read("Host"));
        return nullptr;
    }
};
