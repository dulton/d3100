#pragma once

#include "../fsm.h"
#include "../../libptz/ptz.h"
#include "../../libkvconfig/kvconfig.h"
#include "../udpsrv.h"
#include "../evt.h"
#include "../detector.h"
#include <stdlib.h>

#ifdef WIN32
#	define _USE_MATH_DEFINES
#endif //

#include <math.h>

// 策略说明：
// 机位：一个教师云台 + 教师全景机 ...
// 录播界面：开机进入教师全景 ---〉...
//           教师有目标且视野中间时切教师近景，云台停止转动 ---〉 ...
//           教师走出一定视野范围时切教师全景 ---〉 ...
//           有VGA通知时切换VGA，停止云台转动，VGA超时直接返回searching状态，切教师全景 ...


typedef struct Cal_Angle
{
	double angle_left;//转动到标定区左侧所需角度（弧度）.
	int ptz_left_x;//转动到标定区左侧所需角度（转数）.

	double angle_right;//转动到标定区右侧所需角度（弧度）.
	int ptz_right_x;

	//初始角向右偏为正值.
	//初始角向左偏为负值.
	double angle_init;//跟踪摄像机的初始角（弧度）.
	int ptz_init_x;
	int ptz_init_y;

	double p_left;//标定区左侧x轴坐标.
	double p_right;//标定区右侧x轴坐标.
	//double p;//探测点的x轴坐标.

}Cal_Angle;


/// 聚合了所有功能模块 ..
class p3
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

	double view_threshold_;

	Cal_Angle cal_angle_;

	MovieScene ms_, ms_last_;   // 录播机状态切换 ...

public:
	p3(const char *fname = "teacher_detect_trace.config");
	~p3();

	void run();

	ptz_t *ptz() const { return ptz_; }
	kvconfig_t *cfg() const { return kvc_; }
	FSM *fsm() const { return fsm_; }

	// 根据目标矩形，计算需要转动的转数 ...
	void calc_target_pos(const DetectionEvent::Rect rc, int *x, int *y)
	{	
		// 目标偏离左边偏角(弧度);
		double t_x_angle = target_angle(rc);
		
		// 云台需要转动的角度(弧度);
		double need_x_angle = t_x_angle + cal_angle_.angle_left;

		// 云台需要转动的转数;
	    *x =  (need_x_angle * 180) / (min_angle_ratio_ * M_PI);
		*y = 0;
	}

	// 返回当前云台视角(弧度) ...
	bool view_angle(double &v_angle) const 
	{
		int zoom;
		if(ptz_getzoom(ptz_, &zoom) < 0)
		{
			return false;
		}
		double scale = ptz_ext_zoom2scales(ptz_, zoom);
		v_angle = view_angle_0_ / scale * (M_PI / 180.0);	

		return true;
	}

	// 返回目标偏离“左边”偏角(弧度) ...
	double target_angle(const DetectionEvent::Rect pos) const
	{		
		double ang_left = (fabs)(cal_angle_.angle_left - cal_angle_.angle_init);
		double ang_right = (fabs)(cal_angle_.angle_right - cal_angle_.angle_init);

		double mid_len = (fabs)(cal_angle_.p_right - cal_angle_.p_left) / (tan(ang_left) + tan(ang_right));
		double left_len = mid_len * tan(ang_left);

		double m_l_angle = atan((left_len - ((pos.x + pos.width / 2.0) - cal_angle_.p_left)) / mid_len);
		double angle = ang_left - m_l_angle;

		double mid_x = pos.x + pos.width / 2;

		return angle;
	}

	// 返回云台当前偏离“左边”偏角(弧度) ...
	bool ptz_angle(double &left_angle)
	{
		int x, y;
		if(ptz_getpos(ptz_, &x, &y) < 0)
		{
			return false;
		}
	
		/** FIXME: 有可能出现 h 小于 ptz_left_ 的情况，这个主要是因为云台的“齿数”未必精确 */
		if (x < cal_angle_.ptz_left_x) x = cal_angle_.ptz_left_x;

		left_angle = (x - cal_angle_.ptz_left_x) * min_angle_ratio_ * M_PI / 180.0;	// 转换为弧度;

		return true;		
	}

	// 设定录播机状态 ...
	void set_ms( MovieScene ms ) 
	{
		ms_last_ = ms_; // 暂时未用到 ...
		ms_ =  ms;
	}
	// 切换录播机状态 ...
	void switch_ms()
	{
		ms_switch_to(ms_);
	}

	// vga 超时, 当 now() > vga_back() 时，vga 返回上个状态...
	double vga_back() const { return vga_back_; }

	int vga_last_state() const { return vga_last_state_; }

	void set_vga_last_state( int state) 
	{
		vga_last_state_ = state;
	}

	int get_vga_last_state() const { return vga_last_state_; }

	// 触发 vga
	void set_vga(int last_state) 
	{
		vga_back_ = now() + vga_wait_;
		vga_last_state_ = last_state;
	}

	// 教师无目标超时云台复位 ...
	bool is_reset_; // 是否已复位, 初始化为false ...
	double reset_wait_; // 勿忘从配置文件中读取 ...
	double reset_back_;
	void set_cam_reset(double reset_wait)
	{
		is_reset_ = false;
		reset_back_ = now() + reset_wait;
	}

	// 返回云台等待时间...
	double ptz_wait() const { return ptz_wait_; }
	double ptz_back_;
	int ptz_wait_next_state() const { return ptz_wait_next_state_; }
	void set_ptz_wait(int next_state, double wait = 2.0) 
	{ 
		ptz_wait_ = wait; 
		ptz_back_ = now() + ptz_wait_;
		ptz_wait_next_state_ = next_state;
	}

	// 返回目标是否在视野中，如果在，同时返回偏角...
	// 返回值angle:云台当前位置和目标之间夹角(弧度) ...
	bool isin_field(const DetectionEvent::Rect pos, double &angle)
	{
		double ha_t;
		if(!view_angle(ha_t))
		{
			return false;
		}
		double ha = ha_t / 2.0;

		double pa;
		if(!ptz_angle(pa))
		{
			angle = 0.0;// 无法获取位置不要转动;
			return false;
		}
		ptz_angle(pa);	

		double ta = target_angle(pos);
		angle = ta - pa;
	    
		if((abs(angle) / ha) <= view_threshold_)
		{
            return true;
		}
		else
			return false;
		
	}

	// 根据目标和云台之间偏角，返回转动速度 ...
	bool ptz_speed(double angle, int &speed)
	{
		double ha_t;
		if(!view_angle(ha_t))
		{
			return false;
		}
		double ha = ha_t / 2.0;
		double sa = ha / speeds_.size();// 每段角度 ...
		int idx = angle / sa;

		/* XXX: 正常情况下（目标在视野中时），不会出现idx溢出，但; 
		  		还是加个保证吧 ... */
		if (idx >= speeds_.size()) idx = idx = speeds_.size()-1;

		speed = speeds_[idx];

		return true;
	}

private:
	void load_speeds(const char *conf_str, std::vector<int> &speeds);

	// 读取标定区左右边界x坐标...
	void load_calibration_edge(Cal_Angle &cal_angle);

	// 读取初始化标定参数...
	void load_cal_angle(Cal_Angle &cal_angle);

};


// 下面声明一大堆状态，和状态转换函数 ...
enum state
{
	ST_P3_Staring,	    // 启动后，等待云台归位.
	ST_P3_PtzWaiting,	// 等待云台执行完成 .
	ST_P3_Waiting,	    // 云台已经归位，开始等待udp启动通知.

	ST_P3_Searching,	// 开始等待目标.
	ST_P3_Turnto_Target,// 当找到目标后，转到云台指向目标.

	//ST_P3_Tracking,		// 正在平滑跟踪 .
	ST_P3_Close_Tracking,
	ST_P3_Far_Tracking,

	ST_P3_No_Target,// 教师丢失目标一定时间转换到学生全景.

	ST_P3_Vga,		    // vga，等待10秒后，返回上一个状态.

	ST_P3_End,		    // 结束 .
};


/** 一般情况下，udp 的处理是一样的 */
class p3_common_state: public FSMState
{
public:
	p3_common_state(p3 *p, int id, const char *name)
		: FSMState(id, name)
	{
		p_ = p;
	}

protected:
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Quit) {
			warning("p3", "UDPQuit\n");
			return ST_P3_End;
		}

		if (e->code() == UdpEvent::UDP_Start) {
			info("p3", "to search ....\n");
			// 启动跟踪.
			return ST_P3_Searching;
		}

		if (e->code() == UdpEvent::UDP_Stop) {
			// 结束跟踪.
			ptz_stop(p_->ptz()); // 让云台停止转动 ...
			return ST_P3_Staring;
		}

		if (e->code() == UdpEvent::UDP_VGA) {
			// 无条件到 VGA
			int last_state = id();
			if (last_state == ST_P3_Vga) last_state = p_->vga_last_state();

			p_->set_vga(last_state); // 保存上个非 VGA 状态 ...

			ptz_stop(p_->ptz()); // 让云台停止转动 ...

			// 切换到vga状态...
			p_->set_ms(MS_VGA);
			p_->switch_ms();

			return ST_P3_Vga;
		}

		return FSMState::when_udp(e);
	}

protected:
	p3 *p_;
};


/** 启动，等待云台归位...*/
class p3_starting: public FSMState
{
	p3 *p_;

public:
	p3_starting(p3 *p1)
		: FSMState(ST_P3_Staring, "starting")
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

		p_->set_ptz_wait(ST_P3_Waiting, 2.0);

		return ST_P3_PtzWaiting;
	}
};


/** 等待云台执行完成状态，保存上个状态，等待结束后，返回上个状态 ..
  	此段时间内，不处理其他 ...*/
class p3_ptz_wait: public FSMState
{
	p3 *p_;

public:
	p3_ptz_wait(p3 *p1)
		: FSMState(ST_P3_PtzWaiting, "ptz wait")
	{
		p_ = p1;
	}

	virtual int when_timeout(double curr)
	{
		if (curr > p_->ptz_back_)
			return p_->ptz_wait_next_state();
		return id();
	}
};


/** 云台已经归位，等待udp通知启动 ...*/
class p3_waiting: public FSMState
{
	p3 *p_;

public:
	p3_waiting(p3 *p1)
		: FSMState(ST_P3_Waiting, "waiting udp start")
	{
		p_ = p1;
	}

	virtual int when_timeout(double curr)
	{
		p_->set_ms(MS_TF);
		p_->switch_ms();
		return ST_P3_Searching;//为了便于测试，改动过;
	}

	// 仅仅关心启动和退出事件.
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Start) {
			p_->set_ms(MS_TF); // 设置开机录播机显示画面为教师全景 ...
		    p_->switch_ms();
			return ST_P3_Searching; 
		}
		else if (e->code() == UdpEvent::UDP_Quit) {
			return ST_P3_End; // 将结束主程序.
		}
		else {
			return id();
		}
	}
};


/** 尚未找到目标，等待探测结果 ... */
class p3_searching: public p3_common_state
{
public:
	p3_searching(p3 *p1)
		: p3_common_state(p1, ST_P3_Searching, "searching target")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();

		if (targets.size() == 1) 
		{
			// 单个目标.
			// set_pos 到目标，然后等待转到完成 ...
			int x, y;
			//无法计算转动角度继续搜索状态...
			printf("searching:targets[0].x = %d, targets[0].width = %d&&&&&&\n", targets[0].x, targets[0].width);
			p_->calc_target_pos(targets[0], &x, &y);

			ptz_setpos(p_->ptz(), x, y, 20, 20);

			p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
			return ST_P3_Turnto_Target;
			//===========================
			//该状态也要考虑目标远近采用不同的速度...
			//如果都采用大速度的话会出现转过的现象...
			//int x, y;
			//p_->calc_target_pos(targets[0], &x, &y);

			//double angle;
			//p_->isin_field(targets[0], angle); 	

			//int speed;
			//if(!p_->ptz_speed(fabs(angle), speed))//不在视野内的话speed用最大值;
			//{
			//	return id();
			//}
			//if (speed == 0)
			//{
			//	ptz_stop(p_->ptz());
			//}				
			//else 
			//{
			//	ptz_setpos(p_->ptz(), x, y, speed, speed);
			//}

			//p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));

			//return ST_P1_Turnto_Target;
		}
		else // 此处可复位可不复位吧??? ....
		{
			//无目标云台复位计时开始...
			p_->is_reset_ = false;
			p_->set_cam_reset(p_->reset_wait_);
			return ST_P3_No_Target;
		}
	}
};


/** 教师无目标一定时间云台复位 ...*/
class p3_no_target:public p3_common_state
{
public:
	p3_no_target(p3 *p1)
		: p3_common_state(p1, ST_P3_No_Target, "teacher no target")
	{

	}
	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();

		if(targets.size()  == 1)
		{
			return ST_P3_Searching;
		}
		else
		{
			// 教师无目标超时云台归位吗...
			if(!p_->is_reset_)
			{
				double curr = now();
				if(curr > p_->reset_back_)
				{
					int x0 = atoi(kvc_get(p_->cfg(), "ptz_init_x", "0"));
					int y0 = atoi(kvc_get(p_->cfg(), "ptz_init_y", "0"));
					int z0 = atoi(kvc_get(p_->cfg(), "ptz_init_z", "5000"));

					ptz_setpos(p_->ptz(), x0, y0, 36, 36);	// 快速归位.
					//ptz_setzoom(p_->ptz(), z0);	// 初始倍率.
					p_->is_reset_ = true; // 云台已归位，下次无需再次归位.
				}
			}		
			return id();
		}
	}
};


//** 找到目标，转向目标 */
class p3_turnto_target: public p3_common_state
{
	bool target_valid_;// 目标是否有效.
	DetectionEvent::Rect rc_;// 如果有效，则为目标位置.

public:
	p3_turnto_target(p3 *p1)
		: p3_common_state(p1, ST_P3_Turnto_Target, "turnto target")
	{
		target_valid_ = 0;
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() == 1) 
		{
			target_valid_ = 1;
			rc_ = rcs[0];
		}
		else 
		{
			target_valid_ = 0;
		}

		return id();// 这里不修改当前状态，而是等云台完成后再检查 target_valid_
	}

	virtual int when_ptz_completed(PtzCompleteEvent *e)
	{
		// 云台转到位置, 检查此时目标是否在视野范围内，如果不在，则重新搜索 ...
		double angle;
		int speed;
		if (!target_valid_) 
		{
			printf("p3_turnto_target( target lost! ) **************\n");

			ptz_stop(p_->ptz());
			return ST_P3_Searching;
		}
		else 
		{
			bool is_in_field = p_->isin_field(rc_, angle);

			if(!p_->ptz_speed(fabs(angle), speed))
			{
				printf("p3_turnto_target(cam get speed error !) **************\n");

				return ST_P3_Searching;
			}
				
			// 目标在镜头中间，切教师近景 ...
			if(is_in_field && speed == 0)
			{
				printf("p3_turnto_target(in_field && speed==0) **************\n");

				p_->set_ms(MS_TC);
				p_->switch_ms();

				ptz_stop(p_->ptz());

				return ST_P3_Close_Tracking;
			}
			else// 目标远离中心，保持教师全景状态 ...
			{
				printf("p3_turnto_target(notin_field || speed !=0) **************\n");
				
				p_->set_ms(MS_TF);
				p_->switch_ms();

			    return ST_P3_Far_Tracking;
			}		
		}
	}
};


/** 教师近景跟踪状态 */
class p3_close_tracking: public p3_common_state
{
public:
	p3_close_tracking(p3 *p1)
		: p3_common_state(p1, ST_P3_Close_Tracking, "teacher close tracking")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		// 根据探测结果，当前云台位置，判断是否目标丢失 ...
		double angle;
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() != 1) 
		{
			printf("p3_close_tracking(target lost!) **************\n");

			ptz_stop(p_->ptz());
			return ST_P3_Searching;			
		}
		else 
		{
			if (p_->isin_field(rcs[0], angle)) 
			{			
				return id(); // 若在视野内镜头保持不动 ...
			}
			else// 目标不在视野中 ... 
			{
				printf("p3_close_tracking(target not in field!) **************\n");

				p_->set_ms(MS_TF);
			    p_->switch_ms();

				return ST_P3_Far_Tracking;
			}
		}
	}

};

/** 教师全景跟踪状态 */
class p3_far_tracking: public p3_common_state
{
public:

	p3_far_tracking(p3 *p1)
		: p3_common_state(p1, ST_P3_Far_Tracking, "teacher far tracking")
	{

	}

	virtual int when_detection(DetectionEvent *e)
	{
		// 根据探测结果，当前云台位置，判断是否目标丢失 ...
		double angle;
		int speed;
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() != 1) 
		{
			printf("p3_far_tracking(target lost!) **************\n");

			ptz_stop(p_->ptz());
			return ST_P3_Searching;			
		}
		else 
		{
			bool is_in_field = p_->isin_field(rcs[0], angle);
			if(!p_->ptz_speed(fabs(angle), speed))
			{
				printf("p3_far_tracking(cam get speed error !) **************\n");

				return id();
			}

			if(is_in_field && speed == 0)
			{
				printf("p3_far_tracking(in_field && speed==0) **************\n");

				p_->set_ms(MS_TC);
				p_->switch_ms();

				ptz_stop(p_->ptz());

				return ST_P3_Close_Tracking;
			}
			else
			{
				//p_->set_ms(MS_TF);
				//p_->switch_ms();
				//printf("p3_far_tracking(isin_field) angle=%f,speed=%d **************\n", angle, speed);

				if (angle < 0) ptz_left(p_->ptz(), speed);
				else ptz_right(p_->ptz(), speed);

			    return id();
			}
			
		}

	}

};


/** 处理 VGA ...*/
class p3_vga: public p3_common_state
{
public:
	p3_vga(p3 *p1)
		: p3_common_state(p1, ST_P3_Vga, "vga")
	{
	}

	virtual int when_timeout(double curr)
	{
		if (curr > p_->vga_back()) 
		{    
			// vga超时返回 ...	
			//p_->switch_ms();
			//return p_->get_vga_last_state();

			// vga超时直接切回教师全景，并进入searching状态，无需多余判断 ...
			p_->set_ms(MS_TF);
			p_->switch_ms();

			return ST_P3_Searching;
		}
	}
};
