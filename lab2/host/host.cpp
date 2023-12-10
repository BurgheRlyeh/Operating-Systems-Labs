#include "host.h"

Host Host::hostInstance;

int main(int argc, char* argv[]) {
    openlog("host_log", LOG_PID | LOG_NDELAY, LOG_USER);
    syslog(LOG_INFO, "Logger successfully opened");

    try {
        Host::getInst().start();
    }
    catch (const std::exception& err) {
        syslog(LOG_ERR, "An err %s", err.what());
        closelog();
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Closing logger");
    closelog();
    return EXIT_SUCCESS;
}
