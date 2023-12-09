#pragma once

#include <sys/types.h>

#define MAX_SIZE 2048

class Conn {
public:
	virtual ~Conn() = default;

	static Conn* create(pid_t pid, bool isCreate);

	virtual bool open(pid_t pid, bool isCreate) = 0;

	virtual bool read(void* buf, size_t size) = 0;
	virtual bool write(void* buf, size_t size) = 0;
};
