#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string>
#include "tcp_util.h"
#include "ptz.h"
#include "../teacher_track/log.h"

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

static int send_without_res(ptz_t *ptz, const char *str, const char *func_name)
{
	std::string result;
	if (send_recv(ptz->addr(), ptz->len(), str, result, 1000) < 0) {
		error("ptz", "%s err, cmd='%s'\n", func_name, str);
		return -1;
	}
	
	// TODO: 是否解析 result ???
	return 0;
}

static int send_with_res(ptz_t *ptz, const char *str, std::string &result,
		const char *func_name)
{
	if (send_recv(ptz->addr(), ptz->len(), str, result, 1000) < 0) {
		error("libptz", "%s err, cmd='%s'\n", func_name, str);
		return -1;
	}
	
	// TODO: 是否解析 result ???
	return 0;
}

// url 格式为 tcp://ip:port/who
ptz_t *ptz_open(const char *url)
{
	char ip[16], who[16];
	int port;

	if (sscanf(url, "tcp://%15[^:]:%d/%s", ip, &port, who) != 3) {
		error("libptz", "unknown url fmt: %s\n", url);
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

	return send_without_res(ptz, buf, __func__);
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

	return send_without_res(p, buf, __func__);
}

int ptz_setpos(ptz_t *p, int x, int y, int sx, int sy)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=TurnToPos&Who=%s&X=%d&Y=%d&SX=%d&SY=%d",
			p->who.c_str(), x, y, sx, sy);

	return send_without_res(p, buf, __func__);
}
	 
int ptz_getpos(ptz_t *p, int *x, int *y)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=GetPos&Who=%s",
			p->who.c_str());

	std::string result;
	if (send_with_res(p, buf, result, __func__) < 0) {
		return -1;
	}

	// 返回格式为：X=-880&Y=-300
	if (sscanf(result.c_str(), "X=%d&Y=%d", x, y) != 2) {
		error("libptz", "get_pos res err, '%s'\n", result.c_str());
		return -1;
	}

	return 0;
}

int ptz_zoom_stop(ptz_t *p)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=ZoomStop&Who=%s", p->who.c_str());

	return send_without_res(p, buf, __func__);
}

int ptz_zoom_wide(ptz_t *p, int speed)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=ZoomWide&Who=%s&speed=%d",
			p->who.c_str(), speed);

	return send_without_res(p, buf, __func__);
}

int ptz_zoom_tele(ptz_t *p, int speed)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=ZoomTele&Who=%s&speed=%d",
			p->who.c_str(), speed);

	return send_without_res(p, buf, __func__);
}

int ptz_setzoom(ptz_t *p, int z)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=SetZoom&Who=%s&Zoom=%d",
			p->who.c_str(), z);

	return send_without_res(p, buf, __func__);
}

int ptz_getzoom(ptz_t *p, int *z)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=GetZoom&Who=%s", p->who.c_str());

	std::string result;
	if (send_with_res(p, buf, result, __func__) < 0) {
		return -1;
	}

	// result 格式应该是 Zoom=2133
	if (sscanf(result.c_str(), "Zoom=%d", z) != 1) {
		error("ptz", "libptz] get_zoom ret err, result='%s'\n",
				result.c_str());
		return -1;
	}

	return 0;
}

static int preset_func(ptz_t *p, int id, const char *cmd)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=%s&Who=%s&ID=%d",
			cmd, p->who.c_str(), id);

	return send_without_res(p, buf, __func__);
}

int ptz_preset_clr(ptz_t *p, int id)
{
	return preset_func(p, id, "PresetDel");
}

int ptz_preset_call(ptz_t *p, int id)
{
	return preset_func(p, id, "PresetCall");
}

int ptz_preset_save(ptz_t *p, int id)
{
	return preset_func(p, id, "PresetSave");
}

int ptz_ext_mouse_track(ptz_t *p, float x, float y)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "PtzCmd=MouseTrace&Who=%s&X=%f&Y=%f",
			p->who.c_str(), x, y);

	return send_without_res(p, buf, __func__);
}

