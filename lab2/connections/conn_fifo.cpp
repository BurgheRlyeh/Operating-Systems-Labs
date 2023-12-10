#include "conn_fifo.h"

Conn* Conn::create(pid_t pid, bool isCreate) {
    return new ConnFIFO(pid, isCreate);
}
