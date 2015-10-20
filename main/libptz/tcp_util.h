#pragma once

#include "../teacher_track/runtime.h"

int send_recv(const sockaddr *addr, socklen_t addrlen, const std::string &data, 
		std::string &result, int timeout = 6000);
