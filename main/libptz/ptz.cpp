#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string>
#include "tcp_util.h"
#include "ptz.h"

struct ptz_t
{
	sockaddr_in server_addr;
	std::string who;

	const sockaddr *addr() const
	{
		return (sockaddr*)&server_addr;
	}

	socklen_t len() const
	{
		return sizeof(server_addr);
	}
};

// to impl ptz.h interface

// url 格式为 tcp://ip:port/who
ptz_t *ptz_open(const char *url)
{
	char ip[16], who[16];
	int port;

	if (sscanf(url, "tcp:// %15[^:] : %d / %s", ip, &port, who) != 3) {
		fprintf(stderr, "ERR: [libptz] unknown url fmt: %s\n", url);
		return 0;
	}

	ptz_t *ptz = new ptz_t;
	ptz->server_addr.sin_family = AF_INET;
	ptz->server_addr.sin_port = htons(port);
	ptz->server_addr.sin_addr.s_addr = inet_addr(ip);
	ptz->who = who;

	return ptz;
}

void ptz_close(ptz_t *ptz)
{
	delete ptz;
}

static int ptz_turn(ptz_t *ptz, const char *dir, int speed)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=Turn&Who=%s&Direction=%s&Speed=%d",
			ptz->who.c_str(), dir, speed);

	std::string result;
	if (send_recv(ptz->addr(), ptz->len(), buf, result, 1000) < 0) {
		fprintf(stderr, "ERR: [libptz] %s err\n", dir);
		return -1;
	}
	
	// TODO: 是否解析 result ???
	return 0;
}

int ptz_left(ptz_t *p, int speed)
{
	return ptz_turn(p, "left", speed);
}

int ptz_right(ptz_t *p, int speed)
{
	return ptz_turn(p, "right", speed);
}

int ptz_up(ptz_t *p, int speed)
{
	return ptz_turn(p, "up", speed);
}

int ptz_down(ptz_t *p, int speed)
{
	return ptz_turn(p, "down", speed);
}

int ptz_stop(ptz_t *p)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=StopTurn&Who=%s", 
			p->who.c_str());

	std::string result;
	if (send_recv(p->addr(), p->len(), buf, result) < 0) {
		fprintf(stderr, "ERR: [libptz] stop err\n");
		return -1;
	}

	// TODO: 是否解析 result ???
	return 0;
}

int ptz_setpos(ptz_t *p, int x, int y, int sx, int sy)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=TurnToPos&Who=%s&X=%d&Y=%d&SX=%d&SY=%d",
			p->who.c_str(), x, y, sx, sy);

	std::string result;
	if (send_recv(p->addr(), p->len(), buf, result) < 0) {
		fprintf(stderr, "ERR: [libptz] set_pos err, str='%s'\n", buf);
		return -1;
	}

	// TODO: 是否解析 result ???
	return 0;
}
	 
int ptz_getpos(ptz_t *p, int *x, int *y)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=GetPos&Who=%s",
			p->who.c_str());

	std::string result;
	if (send_recv(p->addr(), p->len(), buf, result) < 0) {
		fprintf(stderr, "ERR: [libptz] get_pos err, str='%s'\n", buf);
		return -1;
	}

	// 返回格式为：X=-880&Y=-300
	if (sscanf(result.c_str(), "X=%d&Y=%d", x, y) != 2) {
		fprintf(stderr, "ERR: [libptz] get_pos res err, '%s'\n", result.c_str());
		return -1;
	}

	return 0;
}

