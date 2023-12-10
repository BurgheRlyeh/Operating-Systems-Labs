#pragma once

#include <atomic>
#include <pthread.h>
#include "../connections/connection.h"

class Host {
    static Host hostInstance;

    std::atomic<pid_t> m_clientPid{ -1 };
    std::atomic<bool> m_isRunning{};

    Connection m_conn;

    Host() {
        struct sigaction sig;
        memset(&sig, 0, sizeof(sig));
        sig.sa_sigaction = sig_handler;
        sig.sa_flags = SA_SIGINFO;

        sigaction(SIGTERM, &sig, nullptr);
        sigaction(SIGUSR1, &sig, nullptr);
    }

    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    static void sig_handler(int sig, siginfo_t* info, void* ptr) {
        syslog(LOG_INFO, "Processing signal %d", sig);

        switch (sig) {
        case SIGUSR1:
            syslog(LOG_INFO, "Client %d connection signal handled", info->si_pid);
            if (Host::getInst().m_clientPid.load() == -1)
                Host::getInst().m_clientPid.store(info->si_pid);
            else
                syslog(LOG_INFO, "Client %d already connected", Host::getInst().m_clientPid.load());
            return;

        case SIGTERM:
            syslog(LOG_INFO, "Termination signal handled");
            Host::getInst().stop();
            return;

        default:
            syslog(LOG_INFO, "Unknown signal handled, skipping");
            return;
        }
    }

public:
    static Host& getInst() {
        return hostInstance;
    }

    void start() {
        syslog(LOG_INFO, "Starting host");
        std::cout << "Host pid = " << getpid() << std::endl;

        typedef void* (*THREADFUNCPTR)(void *);
        pthread_t conn_input;
        pthread_create(&conn_input, nullptr, (THREADFUNCPTR) &Connection::input_loop, &m_conn);

        m_isRunning.store(true);
        while (m_isRunning.load()) {
            if (m_clientPid.load() == -1) continue;
            if (!m_conn.open(m_clientPid.load(), getpid(), true)) {
                m_clientPid.store(-1);
                continue;
            }

            pthread_t conn_job;
            pthread_create(&conn_job, nullptr, (THREADFUNCPTR) &Connection::host_loop, &m_conn);
            sleep(1);

            bool minuteInactive{};
            while (m_isRunning.load() && pthread_tryjoin_np(conn_job, nullptr))
                if (std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::high_resolution_clock::now() - m_conn.getLastMsgTime()
                    ).count() >= 15
                ) {
                minuteInactive = true;
                pthread_cancel(conn_job);
            }
            if (minuteInactive) 
                syslog(LOG_INFO, "Client inactive for 1 minute");
            syslog(LOG_INFO, "Client connection lost");

            m_conn.close();
            syslog(LOG_INFO, "Connection closed");
            
            m_clientPid.store(-1);
        }

        pthread_cancel(conn_input);
        stop();
    }

    void stop() {
        syslog(LOG_INFO, "Terminating");
        m_isRunning.store(false);
        kill(m_clientPid.load(), SIGTERM);
    }
};
