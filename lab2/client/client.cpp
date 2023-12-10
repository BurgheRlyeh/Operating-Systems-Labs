#include "client.h"

Client Client::clientInstance;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Invalid args: specify host pid\n";
        return EXIT_FAILURE;
    }

    openlog("Client", LOG_PID | LOG_NDELAY, LOG_USER);
    syslog(LOG_INFO, "Logger successfully opened");

    syslog(LOG_INFO, "Reading host pid from args");
    int pid;
    try {
        pid = std::stoi(argv[1]);
    } catch (const std::exception& e) {
        syslog(LOG_ERR, "Reading host pid error: %s", e.what());
    	syslog(LOG_INFO, "Closing logger");
        closelog();
        return EXIT_FAILURE;
    }

    try {
        Client::getInst().start(pid);
    }
    catch (const std::exception& e) {
        syslog(LOG_ERR, "Error: %s", e.what());
    	syslog(LOG_INFO, "Closing logger");
        closelog();
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Closing logger");
    closelog();
    return EXIT_SUCCESS;
}
