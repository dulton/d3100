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
	ptz_t *ptz_;		// 仅仅一个教师云台.
	FSM *fsm_;
	udpsrv_t *udp_;		// udp 接收 ...
	detector_t *det_;	// 教师探测模块 ...

	double vga_wait_, vga_back_;	// 配置的 vga 等待时间 ..
	int vga_last_state_; // 切换到vga之前的状态.

	double ptz_scales_;	// 镜头倍率 ..

	double ptz_wait_;	// 云台等待时间，缺省 2秒 .
	int ptz_wait_next_state_;	// 云台等待结束后，返回哪个状态 ...

	std::vector<int> speeds_;	// 0,1,2,4,7...
	double min_angle_ratio_;	// 0.075
	double view_angle_0_;		// 1倍时，镜头水平夹角 .

public:
	p1(const char *fname = "teacher_detect_trace.config");
	~p1();

	void run();

	ptz_t *ptz() const { return ptz_; }
	kvconfig_t *cfg() const { return kvc_; }
	FSM *fsm() const { return fsm_; }

	void calc_target_pos(const DetectionEvent::Rect &rc, int *x, int *y)
	{
		// TODO: 根据目标矩形，计算需要转到的角度.
		*x = 100, *y = -5;
	}

	double view_angle() const 
	{
		// TODO: 返回当前云台视角 ...
		return 1.0;
	}

	double target_angle(const DetectionEvent::Rect &pos) const
	{
		// TODO: 返回目标需要的角度 ....
		return 0.;
	}

	double ptz_angle() const
	{
		// TODO: 返回云台当前偏角 ...
		return 0.;
	}

	// 当 now() > vga_back() 时，vga 返回上个状态.
	double vga_back() const { return vga_back_; }
	int vga_last_state() const { return vga_last_state_; }
	void set_vga(int last_state) // 触发 vga
	{
		vga_back_ = now() + vga_wait_;
		vga_last_state_ = last_state;
	}

	// 返回云台等待时间.
	double ptz_wait() const { return ptz_wait_; }
	int ptz_wait_next_state() const { return ptz_wait_next_state_; }
	void set_ptz_wait(int next_state, double wait = 2.0) 
	{ 
		ptz_wait_ = wait; 
		ptz_wait_next_state_ = next_state;
	}

	// 返回目标是否在视野中，如果在，同时返回偏角 ..
	bool isin_field(const DetectionEvent::Rect &pos, double &angle)
	{
		double ha = view_angle() / 2;
		double ta = target_angle(pos);
		double pa = ptz_angle();

		angle = ta - pa;
		return pa - ha <= ta && ta <= ta + ha;
	}

	// 根据偏角，返回转动速度 ..
	int ptz_speed(double angle)
	{
		double ha = view_angle()/2;
		double sa = ha / speeds_.size();	// 每段角度 ..
		int idx = angle / sa;

		/** XXX: 正常情况下（目标在视野中时），不会出现idx溢出，但 
		  		还是加个保证吧 ... */
		if (idx >= speeds_.size()) idx = idx = speeds_.size()-1;

		return speeds_[idx];
	}

private:
	void load_speeds(const char *conf_str, std::vector<int> &speeds);
};

/// 下面声明一大堆状态，和状态转换函数 ....

enum
{
	ST_P1_Staring,	// 启动后，等待云台归位.
	ST_P1_PtzWaiting,	// 等待云台执行完成 ..
	ST_P1_Waiting,	// 云台已经归位，开始等待udp启动通知.

	ST_P1_Searching,	// 开始等待目标.
	ST_P1_Turnto_Target, // 当找到目标后，转到云台指向目标.
	ST_P1_Tracking,		// 正在平滑跟踪 ...

	ST_P1_Vga,		// vga，等待10秒后，返回上一个状态.

	ST_P1_End,		// 结束 ..
};


/** 一般情况下，udp 的处理是一样的 */
class p1_common_state: public FSMState
{
public:
	p1_common_state(p1 *p, int id, const char *name)
		: FSMState(id, name)
	{
		p_ = p;
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
			// 启动跟踪.
			return ST_P1_Searching;
		}

		if (e->code() == UdpEvent::UDP_Stop) {
			// 结束跟踪.
			return ST_P1_Staring;
		}

		if (e->code() == UdpEvent::UDP_VGA) {
			// 无条件到 VGA
			int last_state = id();
			if (last_state == ST_P1_Vga) last_state = p_->vga_last_state();

			p_->set_vga(last_state); // 保存上个非 VGA 状态 ...

			return ST_P1_Vga;
		}

		return FSMState::when_udp(e);
	}

protected:
	p1 *p_;
};


/** 等待云台执行完成状态，保存上个状态，等待结束后，返回上个状态 ..
  	此段时间内，不处理其他 ...
 */
class p1_ptz_wait: public FSMState
{
	p1 *p_;
	double ptz_back_;

public:
	p1_ptz_wait(p1 *p1)
		: FSMState(ST_P1_PtzWaiting, "ptz wait")
	{
		p_ = p1;
		ptz_back_ = now() + p_->ptz_wait();
	}

	virtual int when_timeout(double curr)
	{
		if (curr > ptz_back_)
			return p_->ptz_wait_next_state();
		return id();
	}
};


/** 启动，等待云台归位.
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

	virtual int when_timeout(double curr)
	{
		int x0 = atoi(kvc_get(p_->cfg(), "ptz_init_x", "0"));
		int y0 = atoi(kvc_get(p_->cfg(), "ptz_init_y", "0"));
		int z0 = atoi(kvc_get(p_->cfg(), "ptz_init_z", "5000"));

		ptz_setpos(p_->ptz(), x0, y0, 36, 36);	// 快速归位.
		ptz_setzoom(p_->ptz(), z0);	// 初始倍率.

		p_->set_ptz_wait(ST_P1_Waiting, 2.0);

		return ST_P1_PtzWaiting;
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

	// 仅仅关心启动和退出事件.
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Start) {
			return ST_P1_Searching; // 
		}
		else if (e->code() == UdpEvent::UDP_Quit) {
			return ST_P1_End; // 将结束主程序.
		}
		else {
			return id();
		}
	}
};


/** 尚未找到目标，等待探测结果 */
class p1_searching: public p1_common_state
{
public:
	p1_searching(p1 *p1)
		: p1_common_state(p1, ST_P1_Searching, "searching target")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();
		if (targets.size() == 1) {
			// 单个目标.
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
	bool target_valid_;	// 目标是否有效.
	DetectionEvent::Rect rc_;	// 如果有效，则为目标位置.

public:
	p1_turnto_target(p1 *p1)
		: p1_common_state(p1, ST_P1_Turnto_Target, "turnto target")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() == 1) {
			target_valid_ = 1;
			rc_ = rcs[0];
		}
		else {
			target_valid_ = 0;
		}

		return id(); // 这里不修改当前状态，而是等云台完成后再检查 target_valid_
	}

	virtual int when_ptz_completed(PtzCompleteEvent *e)
	{
		// 云台转到位置, 检查此时目标是否在视野范围内，如果不在，则重新搜索 ...
		// 
		double angle;
		if (!target_valid_) {
			// 目标丢失 ..
			return ST_P1_Searching;
		}
		else if (p_->isin_field(rc_, angle)) {
			// 目标在视野中，进入稳定跟踪状态 ..
			return ST_P1_Tracking;
		}
		else {
			// 目标已经离开视野，则 set_pos .
			int x, y;
			p_->calc_target_pos(rc_, &x, &y);
			ptz_setpos(p_->ptz(), x, y, 36, 36);
			p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
			return ST_P1_Turnto_Target;
		}
	}
};


/** 处理 VGA */
class p1_vga: public p1_common_state
{
public:
	p1_vga(p1 *p1)
		: p1_common_state(p1, ST_P1_Vga, "vga")
	{
	}

	virtual int when_timeout(double curr)
	{
		if (curr > p_->vga_back()) {    // 检查是否vga 超时，超时则返回上一个状态 ..
			return p_->vga_last_state();
		}

		return id();
	}
};


/** 稳定跟踪状态 */
class p1_tracking: public p1_common_state
{
public:
	p1_tracking(p1 *p1)
		: p1_common_state(p1, ST_P1_Tracking, "tracking")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		// 根据探测结果，当前云台位置，判断是否目标丢失 ....
		double angle;
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() != 1) {
			return ST_P1_Searching;
		}
		else {
			// 若在视野中，根据 angle 决定如何左右转 ..
			int speed = p_->ptz_speed(abs(angle));
			if (speed == 0)
				ptz_stop(p_->ptz());
			else {
				if (angle < 0) ptz_left(p_->ptz(), speed);
				else ptz_right(p_->ptz(), speed);
			}

			return id();
		}
	}
};

