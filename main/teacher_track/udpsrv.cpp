#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include "udpsrv.h"
#include "log.h"

/** 使用一个独立的工作线程，bind 0.0.0.0:8642
  	解析收到的命令，生成 UdpEvent push 到 fsm 中
 */

struct udpsrv_t
{
	int fd, quit;
	pthread_t th;
	FSM *fsm;
};

static void parse_and_handle(FSM *fsm, unsigned char buf[], int len)
{
	FSMEvent *evt = 0;

	if (len == 2) {
		if (buf[0] == 1 && buf[1] == 1) {
			evt = new UdpEvent(UdpEvent::UDP_Start);
		}
		else if (buf[0] == 1 && (buf[1] == 2 || buf[1] == 3)) {
			evt = new UdpEvent(UdpEvent::UDP_Stop);
		}
		else if (buf[0] == 0xff && buf[1] == 0xff) {
			evt = new UdpEvent(UdpEvent::UDP_VGA);
		}
		else if (buf[0] == 0 && buf[1] == 0) {
			evt = new UdpEvent(UdpEvent::UDP_Quit); // XXX: for testing
		}
		else {
			warning("udpsrv", "unknown udp cmd: '%02x %02x'\n",
					(unsigned char)buf[0], (unsigned char)buf[1]);
		}
	}

	if (evt && fsm) {
		fsm->push_event(evt);
	}
}

static void *thread_proc(void *arg)
{
	udpsrv_t *p = (udpsrv_t*)arg;

	fd_set rds;
	FD_ZERO(&rds);
	FD_SET(p->fd, &rds);

	fcntl(p->fd, F_SETFL, O_NONBLOCK | fcntl(p->fd, F_GETFL, 0));

	while (!p->quit) {
		struct timeval tv = { 0, 100*1000 };
		fd_set rd = rds;

		if (select(p->fd+1, &rd, 0, 0, &tv) > 0) {
			if (FD_ISSET(p->fd, &rd)) {
				char buf[1024];
				sockaddr_in from;
				socklen_t len = sizeof(from);
				int rc = recvfrom(p->fd, buf, 1024, 0, (sockaddr*)&from, &len);
				parse_and_handle(p->fsm, (unsigned char*)buf, rc);
			}
		}
	}
}

udpsrv_t *us_open(FSM *fsm, int port, const char *bip)
{
	udpsrv_t *us = new udpsrv_t;
	if (!us) return 0;

	us->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (us->fd < 0) {
		error("udpsrv", "create socket err!\n");
		delete us;
		return 0;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(bip);
	if (bind(us->fd, (sockaddr*)&sin, sizeof(sin)) < 0) {
		error("udpsv", "bind %s:%d err\n", bip, port);
		close(us->fd);
		delete us;
		return 0;
	}

	us->quit = 0;	// 用于结束工作线程
	if (0 != pthread_create(&us->th, 0, thread_proc, us)) {
		error("udpsrv", "can't start work thread\n");
		close(us->fd);
		delete us;
		return 0;
	}

	us->fsm = fsm;
	return us;
}

void us_close(udpsrv_t *us)
{
	us->quit = 1;
	void *r;
	pthread_join(us->th, &r);
	close(us->fd);
	delete us;
}

