#include "conn_mmap.h"

Conn* Conn::create(pid_t pid, bool isCreate) {
    return new ConnMMAP(pid, isCreate);
}
