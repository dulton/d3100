#pragma once

#include <arpa/inet.h>

int send_recv(const sockaddr *addr, socklen_t addrlen, const std::string &data, 
		std::string &result, int timeout = 1000);
