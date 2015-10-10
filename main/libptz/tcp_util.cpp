#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include "tcp_util.h"


/** 发送，并接收返回，根据超时
  	每次创建一个 tcp socket
 */
int send_recv(const sockaddr *addr, socklen_t addrlen, const std::string &data, 
		std::string &result, int timeout)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "ERR: [libptz] can't create socket, %d\n", errno);
		return -1;
	}

	do {
		// non block 
		int flags = fcntl(fd, F_GETFL, 0);
		flags |= O_NONBLOCK;
		fcntl(fd, F_SETFL, flags);
	} while (0);

	if (connect(fd, addr, addrlen) == -1) {
		if (errno != EINPROGRESS) {
			fprintf(stderr, "ERR: [libptz] can't connect server, %d\n", errno);
			close(fd);
			return -1;
		}

		fd_set ws;
		FD_ZERO(&ws);
		FD_SET(fd, &ws);
		timeval tv = { timeout / 1000, timeout % 1000 * 1000 };
		if (select(fd+1, 0, &ws, 0, &tv) <= 0) {
			fprintf(stderr, "ERR: [libptz] can't connect server, %d\n", errno);
			close(fd);
			return -1;
		}
	
		if (!FD_SET(fd, &ws)) {
			fprintf(stderr, "ERR: [libptz] ????\n");
		}
	}

	// FIXME: 一般 data 的长度比较小，不会溢出
	send(fd, data.data(), data.size(), 0);

	do {
		fd_set rs;
		FD_ZERO(&rs);
		FD_SET(fd, &rs);
		timeval tv = { timeout / 1000, timeout % 1000 * 1000 };
		if (select(fd+1, &rs, 0, 0, &tv) <= 0) {
			fprintf(stderr, "ERR: [libptz] wait server res timeout!\n");
			close(fd);
			return -1;
		}

		if (!FD_SET(fd, &rs)) {
			fprintf(stderr, "ERR: [libptz] ?????\n");
		}

		char buf[128];	// FIXME: 返回字节一般也很少，也没有考虑截断 ...
		int rc = recv(fd, buf, sizeof(buf), 0);
		if (rc < 0) {
			fprintf(stderr, "ERR: [libptz] recv err?? %d\n", errno);
			close(fd);
			return -1;
		}

		result = std::string(buf, rc);
	} while (0);

	close(fd);

	return 0;
}

