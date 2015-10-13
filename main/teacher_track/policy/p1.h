#pragma once

#include "../fsm.h"
#include "../../libptz/ptz.h"
#include "../../libkvconfig/kvconfig.h"
#include "../../libteacher_detect/detect.h"
#include "../evt.h"

/// 这个相当于 fsm.h 中的 opaque
class p1
{
	kvconfig_t *kvc_;	// 
	ptz_t *ptz_;		// 仅仅一个教师云台
	detect_t *det_;		// 探测模块
	FSM *fsm_;

public:
	p1(const char *fname = "teacher_detect_trace.config");
	~p1();

	void run();

	ptz_t *ptz() const { return ptz_; }
	kvconfig_t *cfg() const { return kvc_; }
	detect_t *detector() const { return det_; }

private:
	static void boot(void *opaque)
	{
		((p1*)opaque)->boot();
	}

	void boot(); // 启动函数，一般是个云台归位 ...
};

/// 下面声明一大堆状态，和状态转换函数 ....

enum
{
	ST_P1_Staring,	// 启动后，等待云台归位
	ST_P1_Inited,	// 云台已经归位，开始等待探测结果

	ST_P1_End,		// 结束 ..
};

class p1_starting_timeout: public FSMTransition
{
public:
	p1_starting_timeout(): FSMTransition(ST_P1_Staring, EVT_Timeout)
	{
	}

	virtual int operator()(void *opaque, FSMEvent *evt)
	{
		p1 *ctx = (p1*)opaque;

		/// 说明云台转动已经到位，此处可以调用 get_pos 验证一下
		int x0 = atoi(kvc_get(ctx->cfg(), "ptz_init_x", "0"));
		int y0 = atoi(kvc_get(ctx->cfg(), "ptz_init_y", "0"));
		int x, y;
		ptz_getpos(ctx->ptz(), &x, &y);

		if (x0 == x && y0 == y) {
			// FIXME: 应该有误差的，只是为了说明意思 ...
			return ST_P1_Inited;
		}

		return state_;
	}
};

class p1_quit: public FSMTransition
{
public:
	p1_quit(int st): FSMTransition(st, EVT_Quit)
	{
	}

	virtual int operator()(void *opaque, FSMEvent *evt)
	{
		// 只要返回 ST_P1_End，整个状态机将结束
		return ST_P1_End;
	}
};

class p1_inited_search_target: public FSMTransition
{
public:
	p1_inited_search_target(): FSMTransition(ST_P1_Inited, EVT_Detection)
	{
	}

	virtual int operator()(void *opaque, FSMEvent *evt)
	{
		p1 *ctx = (p1*)opaque;
		DetectionEvent *det_result = (DetectionEvent*)evt;

		/// TODO: 根据探测结果做出相应动作 ...

		return state_;
	}
};

class p1_inited_recv_udp: public FSMTransition
{
public:
	p1_inited_recv_udp(): FSMTransition(ST_P1_Inited, EVT_Udp)
	{
	}

	virtual int operator()(void *opaque, FSMEvent *evt)
	{
		p1 *ctx = (p1*)opaque;
		UdpEvent *ue = (UdpEvent*)evt;

		// TODO: 根据 udp code 决定下一步，如停止，如VGA， ....
		switch (ue->udpcode_) {
		case 0:		
			break;
		}

		return state_;
	}
};

