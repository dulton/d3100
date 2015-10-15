#pragma once

#include "../fsm.h"
#include "../../libptz/ptz.h"
#include "../../libkvconfig/kvconfig.h"
#include "../udpsrv.h"
#include "../evt.h"
#include "../detector.h"

/// 聚合了所有功能模块 ..
class p1
{
	kvconfig_t *kvc_;	// 
	ptz_t *ptz_;		// 仅仅一个教师云台
	FSM *fsm_;
	udpsrv_t *udp_;		// udp 接收 ...
	detector_t *det_;	// 教师探测模块 ...

public:
	p1(const char *fname = "teacher_detect_trace.config");
	~p1();

	void run();

	ptz_t *ptz() const { return ptz_; }
	kvconfig_t *cfg() const { return kvc_; }
	FSM *fsm() const { return fsm_; }

	void calc_target_pos(const DetectionEvent::Rect &rc, int *x, int *y)
	{
		// TODO: 根据目标矩形，计算需要转到的角度
		*x = 100, *y = -5;
	}

private:
};

/// 下面声明一大堆状态，和状态转换函数 ....

enum
{
	ST_P1_Staring,	// 启动后，等待云台归位
	ST_P1_Waiting,	// 云台已经归位，开始等待udp启动通知

	ST_P1_Searching,	// 开始等待目标
	ST_P1_Turnto_Target, // 当找到目标后，转到云台指向目标
	ST_P1_Tracking,		// 正在平滑跟踪 ...

	ST_P1_Vga,		// vga，等待10秒后，返回上一个状态

	ST_P1_End,		// 结束 ..
};


/** 一般情况下，udp 的处理是一样的 */
class p1_common_state: public FSMState
{
public:
	p1_common_state(int id, const char *name)
		: FSMState(id, name)
	{
	}

protected:
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Quit) {
			warning("p1", "UDPQuit\n");
			return ST_P1_End;
		}

		if (e->code() == UdpEvent::UDP_Start) {
			info("p1", "to search ....\n");
			// 启动跟踪，
			return ST_P1_Searching;
		}

		if (e->code() == UdpEvent::UDP_Stop) {
			// 结束跟踪，
			return ST_P1_Staring;
		}

		if (e->code() == UdpEvent::UDP_VGA) {
			// 无条件到 VGA
			return ST_P1_Vga;
		}

		return FSMState::when_udp(e);
	}
};


/** 启动，等待云台归位
 */
class p1_starting: public FSMState
{
	p1 *p_;

public:
	p1_starting(p1 *p1)
		: FSMState(ST_P1_Staring, "starting")
	{
		p_ = p1;
	}

	virtual void when_enter()
	{
		int x0 = atoi(kvc_get(p_->cfg(), "ptz_init_x", "0"));
		int y0 = atoi(kvc_get(p_->cfg(), "ptz_init_y", "0"));
		int z0 = atoi(kvc_get(p_->cfg(), "ptz_init_z", "5000"));

		ptz_setpos(p_->ptz(), x0, y0, 36, 36);	// 快速归位
		ptz_setzoom(p_->ptz(), z0);	// 初始倍率

		// 给一个超时 ...
		p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_zoom&pos"));
	}

	virtual int when_ptz_completed(PtzCompleteEvent *evt)
	{
		// TODO: 调用 get_pos/get_zoom 检查是否到位 ...
		return ST_P1_Waiting;
	}
};


/** 云台已经归位，等待udp通知启动 ...
 */
class p1_waiting: public FSMState
{
	p1 *p_;

public:
	p1_waiting(p1 *p1)
		: FSMState(ST_P1_Waiting, "waiting udp start")
	{
		p_ = p1;
	}

	// 仅仅关心启动和退出事件
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Start) {
			return ST_P1_Searching; // 
		}
		else if (e->code() == UdpEvent::UDP_Quit) {
			return ST_P1_End; // 将结束主程序
		}
		else {
			return id();
		}
	}
};


/** 尚未找到目标，等待探测结果 */
class p1_searching: public p1_common_state
{
	p1 *p_;

public:
	p1_searching(p1 *p1)
		: p1_common_state(ST_P1_Searching, "searching target")
	{
		p_ = p1;
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();
		if (targets.size() == 1) {
			// 单个目标
			// set_pos 到目标，然后等待转到完成 ...
			int x, y;
			p_->calc_target_pos(targets[0], &x, &y);
			ptz_setpos(p_->ptz(), x, y, 36, 36);
			p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
			
			return ST_P1_Turnto_Target;
		}
		else {
			return id();
		}
	}
};

class p1_turnto_target: public p1_common_state
{
	p1 *p_;
	bool target_valid_;	// 目标是否有效？
	DetectionEvent::Rect rc_;	// 如果有效，则为目标位置

public:
	p1_turnto_target(p1 *p1)
		: p1_common_state(ST_P1_Turnto_Target, "turnto target")
	{
		p_ = p1;
	}

	virtual int when_detection(DetectionEvent *e)
	{
		// 继续处理探测结果
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() == 1) {
			target_valid_ = 1;
			rc_ = rcs[0];
		}
		else {
			target_valid_ = 0;
		}

		return id(); // FIXME: 这里不修改当前状态，而是等云台完成后再检查 target_valid_
	}

	virtual int when_ptz_completed(PtzCompleteEvent *e)
	{
		// TODO: 云台转到位置, 检查此时目标是否在视野范围内，如果不在，则重新搜索 ...
		// 
		return id();
	}
};


/** 处理 VGA */
class p1_vga: public p1_common_state
{
	p1 *p_;

public:
	p1_vga(p1 *p1)
		: p1_common_state(ST_P1_Vga, "vga")
	{
		p_ = p1;
	}
};


/** 稳定跟踪状态 */
class p1_tracking: public p1_common_state
{
	p1 *p_;

public:
	p1_tracking(p1 *p1)
		: p1_common_state(ST_P1_Tracking, "tracking")
	{
		p_ = p1;
	}

	virtual int when_detection(DetectionEvent *e)
	{
		// TODO: 根据探测结果，当前云台位置，判断是否目标丢失 ....
		return id();
	}
};

