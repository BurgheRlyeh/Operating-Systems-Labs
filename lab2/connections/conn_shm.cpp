#include "conn_shm.h"

Conn* Conn::create(pid_t pid, bool isCreate) {
    return new ConnSHM(pid, isCreate);
}
