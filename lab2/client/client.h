#pragma once

#include <atomic>
#include <pthread.h>
#include "../connections/connection.h"

class Client {
    static Client clientInstance;

    std::atomic<bool> m_isHostOk{};
    std::atomic<bool> m_isRunning{};

    Connection m_conn{};

    Client() {
        struct sigaction sig;
        memset(&sig, 0, sizeof(sig));
        sig.sa_sigaction = sig_handler;
        sig.sa_flags = SA_SIGINFO;

        sigaction(SIGTERM, &sig, nullptr);
        sigaction(SIGUSR1, &sig, nullptr);
    }

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    static void sig_handler(int sig, siginfo_t* info, void* ptr) {
        syslog(LOG_INFO, "Processing signal %d", sig);
    
        switch (sig) {
        case SIGUSR1:
            syslog(LOG_INFO, "Host readiness signal handled");
            Client::getInst().m_isHostOk.store(true);
            return;
    
        case SIGTERM:
            syslog(LOG_INFO, "Termination signal handled");
            Client::getInst().stop();
            return;

        default:
            syslog(LOG_INFO, "Unknown signal handled, skipping");
            return;
        }
    }

public:
    static Client& getInst() {
        return clientInstance;
    }

    void start(pid_t pidHost) {
        syslog(LOG_INFO, "Starting client");
        m_isRunning.store(true);

        if (kill(pidHost, SIGUSR1) != 0) {
            syslog(LOG_ERR, "Host %d not available", pidHost);
            stop();
            return;
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        while (!m_isHostOk.load()) {
            if (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - t0
                ).count() >= 5
            ) {
                syslog(LOG_ERR, "Host cannot connect for 5 seconds");
                stop();
                return;
            }
        }
        
        typedef void * (*THREADFUNCPTR)(void *);
        pthread_t conn_input;
        pthread_create(&conn_input, nullptr, (THREADFUNCPTR) &Connection::input_loop, &m_conn);

        if (!m_conn.open(getpid(), pidHost, false)) {
            stop();
            return;
        }

        pthread_t conn_job;
        pthread_create(&conn_job, nullptr, (THREADFUNCPTR) &Connection::client_loop, &m_conn);
        sleep(1);

        while (m_isRunning.load() && pthread_tryjoin_np(conn_job, nullptr));
        
        pthread_cancel(conn_job);
        pthread_cancel(conn_input);

        stop();
    }

    void stop() {
        syslog(LOG_INFO, "Terminating");
        m_isRunning.store(false);
    }
};
