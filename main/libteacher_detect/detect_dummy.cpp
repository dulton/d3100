/** 模拟 teacher_detect 模块，简单的从网络接收数据，并作为 det_detect() 的返回.
 */

#include <stdio.h>
#include <string>
#include "../teacher_track/runtime.h"
#include "detect.h"

const char *_empty_result = "{\"stamp\":12345,\"rect\":[]}"; 

#define MULTICAST_ADDR "239.119.119.1"
#define PORT 11000

struct detect_t
{
	int fd;	// 用于接收udp数据.
	std::string result;
};

detect_t *det_open(const char *fname)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(stderr, "ERR: [detect] can't create socket?\n");
		return 0;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(11000);
	sin.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(fd, (sockaddr*)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "ERR: [detect] bind %d error!\n", PORT);
		return 0;
	}

	fprintf(stdout, "start UDP port %d\n", PORT);

	// 加入组播，这样方便小杨发送探测消息 ..
	struct ip_mreq req;
	req.imr_interface.s_addr = inet_addr("0.0.0.0");
	req.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
	setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&req, sizeof(req));

	set_sock_nonblock(fd);

	detect_t *d = new detect_t;
	d->result = _empty_result;
	d->fd = fd;

	return d;
}

void det_close(detect_t *det)
{
	closesocket(det->fd);
	delete det;
}

const char *det_detect(detect_t *det)
{
	/** 接收 udp，100ms 超时，返回 det->result */
	fd_set rs;
	FD_ZERO(&rs);
	FD_SET(det->fd, &rs);

	timeval tv = { 0, 100*100 }; // 100ms

	if (select(det->fd+1, &rs, 0, 0, &tv) == 1 && FD_ISSET(det->fd, &rs)) {
		char buf[64*1024];
		sockaddr_in from;
		socklen_t size = sizeof(from);

		int len = recvfrom(det->fd, buf, sizeof(buf), 0, (sockaddr*)&from, &size);
		if (len < 0) {
			fprintf(stderr, "ERR: [detect] recvfrom ERR???\n");
		}
		else {
			// 更新.
			det->result = std::string(buf, len);
		}
	}

	return det->result.c_str();
}

