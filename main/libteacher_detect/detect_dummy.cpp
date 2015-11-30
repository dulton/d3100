/** 模拟 teacher_detect 模块，简单的从网络接收数据，并作为 det_detect() 的返回.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "../teacher_track/runtime.h"
#include "../teacher_track/log.h"
#include "detect.h"

#ifdef WIN32

#else
#include <unistd.h>
#endif

const char *_empty_result = "{\"stamp\":0000,\"rect\":[]}"; 

#define MULTICAST_ADDR "239.119.119.1"
#define PORT 11000

struct detect_t
{
	int fd;	// 用于接收udp数据.
	std::string result;
};

detect_t *det_open(const char *fname)
{
	fprintf(stderr, "%s %s %d\n", __FILE__, __func__, __LINE__);
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		fatal("det dummy", "can't create socket?\n");
		return 0;
	}

	char *port = getenv("TT_PORT");

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port ? atoi(port) : 11000);
	sin.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (sockaddr*)&sin, sizeof(sin)) < 0) {
		fatal("det dummy", "bind %d error!\n", PORT);
		return 0;
	}

	info("det dummy", "start UDP port %d\n", PORT);

//	// 加入组播，这样方便小杨发送探测消息 ..
//	struct ip_mreq req;
//	req.imr_interface.s_addr = inet_addr("172.16.1.13");
//	req.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
//	setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&req, sizeof(req));
//	info("det dummy", "join multicast addr %s\n", MULTICAST_ADDR);

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

const char *det_detect(detect_t *det, const hiVIDEO_FRAME_INFO_S *notused)
{
	/** 接收 udp，500ms 超时，返回 det->result */
	fd_set rs;
	FD_ZERO(&rs);
	FD_SET(det->fd, &rs);

	fprintf(stderr, "DEBUG: %s.%d\n", __func__, __LINE__);

	struct timeval tv = { 0, 500*1000 }; // 

	if (select(det->fd+1, &rs, 0, 0, &tv) == 1 && FD_ISSET(det->fd, &rs)) {
	fprintf(stderr, "DEBUG: %s.%d\n", __func__, __LINE__);
		char buf[1024];
		sockaddr_in from;
		socklen_t size = sizeof(from);

		int len = recvfrom(det->fd, buf, sizeof(buf), 0, (sockaddr*)&from, &size);
	fprintf(stderr, "DEBUG: %s.%d\n", __func__, __LINE__);
		if (len < 0) {
	fprintf(stderr, "DEBUG: %s.%d\n", __func__, __LINE__);
			error("det dummy", "recvfrom ERR???\n");
		}
		else {
	fprintf(stderr, "DEBUG: %s.%d\n", __func__, __LINE__);
			if (buf[0] == '1' || buf[0] == '2' || buf[0] == '3') {
				/** 1: 教师，2: 学生，3: 板书 ..
				 */
				debug("det dummy", "\"%s\"\n", buf);
				det->result = std::string(buf+1, len-1);
			}
			else {
				det->result = std::string(buf, len);
			}
		}
	}

	return det->result.c_str();
}

