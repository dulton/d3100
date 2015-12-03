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
#endif 

#include <math.h>


// ����˵����
// ��λ��һ����ʦ��̨ ...
// ¼�����棺���������ʦ��������VGA֪ͨʱ�л�VGA��������һ��״̬��VGA��ʱ�лؽ�ʦ��������������һ��״̬ ...


typedef struct Cal_Angle_2
{
	double angle_left;//ת�����궨���������Ƕȣ����ȣ�.
	int ptz_left_x;//ת�����궨���������Ƕȣ�ת����.

	double angle_right;//ת�����궨���Ҳ�����Ƕȣ����ȣ�.
	int ptz_right_x;

	//��ʼ������ƫΪ��ֵ.
	//��ʼ������ƫΪ��ֵ.
	double angle_init;//����������ĳ�ʼ�ǣ����ȣ�.
	int ptz_init_x;
	int ptz_init_y;

	double p_left;//�궨�����x������.
	double p_right;//�궨���Ҳ�x������.
	//double p;//̽����x������.

}Cal_Angle_2;


/// �ۺ������й���ģ�� ..
class p2
{
	kvconfig_t *kvc_;	// 
	ptz_t *ptz_;		// ����һ����ʦ��̨.
	FSM *fsm_;
	udpsrv_t *udp_;		// udp ���� ...
	detector_t *det_;	// ��ʦ̽��ģ�� ...

	double vga_wait_, vga_back_;	// ���õ� vga �ȴ�ʱ�� ..
	int vga_last_state_; // �л���vga֮ǰ��״̬.

	double ptz_scales_;	// ��ͷ���� ..

	double ptz_wait_;	// ��̨�ȴ�ʱ�䣬ȱʡ 2�� .
	int ptz_wait_next_state_;	// ��̨�ȴ������󣬷����ĸ�״̬ ...

	std::vector<int> speeds_;	// 0,1,2,4,7...
	double min_angle_ratio_;	// 0.075
	double view_angle_0_;		// 1��ʱ����ͷˮƽ�н� .

	Cal_Angle_2 cal_angle_;

	MovieScene ms_, ms_last_;   // ¼����״̬�л� ...

public:
	p2(const char *fname = "teacher_detect_trace.config");
	~p2();

	void run();

	ptz_t *ptz() const { return ptz_; }
	kvconfig_t *cfg() const { return kvc_; }
	FSM *fsm() const { return fsm_; }

	// ����Ŀ����Σ�������Ҫת����ת�� ...
	void calc_target_pos(const DetectionEvent::Rect rc, int *x, int *y)
	{	
		// Ŀ��ƫ�����ƫ��(����);
		double t_x_angle = target_angle(rc);
		
		// ��̨��Ҫת���ĽǶ�(����);
		double need_x_angle = t_x_angle + cal_angle_.angle_left;

		// ��̨��Ҫת����ת��;
	    *x =  (need_x_angle * 180) / (min_angle_ratio_ * M_PI);
		*y = 0;
	}

	// ���ص�ǰ��̨�ӽ�(����) ...
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

	// ����Ŀ��ƫ�롰��ߡ�ƫ��(����) ...
	double target_angle(const DetectionEvent::Rect pos) const
	{		
		double ang_left = (fabs)(cal_angle_.angle_left - cal_angle_.angle_init);
		double ang_right = (fabs)(cal_angle_.angle_right - cal_angle_.angle_init);

		double mid_len = (fabs)(cal_angle_.p_right - cal_angle_.p_left) / (tan(ang_left) + tan(ang_right));
		double left_len = mid_len * tan(ang_left);

		double m_l_angle = atan((left_len - ((pos.x + pos.width / 2.0) - cal_angle_.p_left)) / mid_len);
		double angle = ang_left - m_l_angle;

		//double mid_x = pos.x + pos.width / 2;

		return angle;
	}

	// ������̨��ǰƫ�롰��ߡ�ƫ��(����) ...
	bool ptz_angle(double &left_angle)
	{
		int x, y;
		if(ptz_getpos(ptz_, &x, &y) < 0)
		{
			return false;
		}
	
		/** FIXME: �п��ܳ��� h С�� ptz_left_ ������������Ҫ����Ϊ��̨�ġ�������δ�ؾ�ȷ */
		if (x < cal_angle_.ptz_left_x) x = cal_angle_.ptz_left_x;

		left_angle = (x - cal_angle_.ptz_left_x) * min_angle_ratio_ * M_PI / 180.0;	// ת��Ϊ����;

		return true;		
	}

	// �趨¼����״̬ ...
	void set_ms( MovieScene ms ) 
	{
		ms_last_ = ms_; // ��ʱδ�õ� ...
		ms_ =  ms;
	}
	// �л�¼����״̬ ...
	void switch_ms()
	{
		ms_switch_to(ms_);
	}

	// vga ��ʱ, �� now() > vga_back() ʱ��vga �����ϸ�״̬...
	double vga_back() const { return vga_back_; }

	int vga_last_state() const { return vga_last_state_; }

	void set_vga_last_state( int state) 
	{
		vga_last_state_ = state;
	}

	int get_vga_last_state() const { return vga_last_state_; }

	// ���� vga
	void set_vga(int last_state) 
	{
		vga_back_ = now() + vga_wait_;
		vga_last_state_ = last_state;
	}

	// ��ʦ��Ŀ�곬ʱ��̨��λ ...
	bool is_reset_; // �Ƿ��Ѹ�λ, ��ʼ��Ϊfalse ...
	double reset_wait_; // �����������ļ��ж�ȡ ...
	double reset_back_;
	void set_cam_reset(double reset_wait)
	{
		is_reset_ = false;
		reset_back_ = now() + reset_wait;
	}

	// ������̨�ȴ�ʱ��...
	double ptz_wait() const { return ptz_wait_; }
	double ptz_back_;
	int ptz_wait_next_state() const { return ptz_wait_next_state_; }
	void set_ptz_wait(int next_state, double wait = 2.0) 
	{ 
		ptz_wait_ = wait; 
		ptz_back_ = now() + ptz_wait_;
		ptz_wait_next_state_ = next_state;
	}

	// ����Ŀ���Ƿ�����Ұ�У�����ڣ�ͬʱ����ƫ��...
	// ����ֵangle:��̨��ǰλ�ú�Ŀ��֮��н�(����) ...
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
			angle = 0.0;// �޷���ȡλ�ò�Ҫת��;
			return false;
		}
		ptz_angle(pa);	

		double ta = target_angle(pos);
		angle = ta - pa;
	
		return pa - ha <= ta && ta <= ta + ha;
	}

	// ����Ŀ�����̨֮��ƫ�ǣ�����ת���ٶ� ...
	bool ptz_speed(double angle, int &speed)
	{
		double ha_t;
		if(!view_angle(ha_t))
		{
			return false;
		}
		double ha = ha_t / 2.0;
		double sa = ha / speeds_.size();// ÿ�νǶ� ...
		int idx = angle / sa;

		/* XXX: ��������£�Ŀ������Ұ��ʱ�����������idx�������; 
		  		���ǼӸ���֤�� ... */
		if (idx >= speeds_.size()) idx = idx = speeds_.size()-1;

		speed = speeds_[idx];

		return true;
	}

private:
	void load_speeds(const char *conf_str, std::vector<int> &speeds);

	// ��ȡ�궨�����ұ߽�x����...
	void load_calibration_edge(Cal_Angle_2 &cal_angle);

	// ��ȡ��ʼ���궨����...
	void load_cal_angle(Cal_Angle_2 &cal_angle);

};


// ��������һ���״̬����״̬ת������ ...
enum state_2
{
	ST_P2_Staring,	    // �����󣬵ȴ���̨��λ.
	ST_P2_PtzWaiting,	// �ȴ���ִ̨����� .
	ST_P2_Waiting,	    // ��̨�Ѿ���λ����ʼ�ȴ�udp����֪ͨ.

	ST_P2_Searching,	// ��ʼ�ȴ�Ŀ��.
    ST_P2_No_Target,    // ��ʦ����Ŀ��.
	ST_P2_Turnto_Target,// ���ҵ�Ŀ���ת����ָ̨��Ŀ��.
	ST_P2_Tracking,		// ����ƽ������ .

	ST_P2_Vga,		    // vga���ȴ�10��󣬷�����һ��״̬.

	ST_P2_End,		    // ���� .
};


/** һ������£�udp �Ĵ�����һ���� */
class p2_common_state: public FSMState
{
public:
	p2_common_state(p2 *p, int id, const char *name)
		: FSMState(id, name)
	{
		p_ = p;
	}

protected:
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Quit) {
			warning("p1", "UDPQuit\n");
			return ST_P2_End;
		}

		if (e->code() == UdpEvent::UDP_Start) {
			info("p1", "to search ....\n");
			// ��������.
			return ST_P2_Searching;
		}

		if (e->code() == UdpEvent::UDP_Stop) {
			// ��������.
			ptz_stop(p_->ptz()); // ����ֹ̨ͣת�� ...
			return ST_P2_Staring;
		}

		if (e->code() == UdpEvent::UDP_VGA) {
			// �������� VGA
			int last_state = id();
			if (last_state == ST_P2_Vga) last_state = p_->vga_last_state();

			p_->set_vga(last_state); // �����ϸ��� VGA ״̬ ...

			ptz_stop(p_->ptz()); // ����ֹ̨ͣת�� ...

			// �л���vga״̬...
			p_->set_ms(MS_VGA);
			p_->switch_ms();

			return ST_P2_Vga;
		}

		return FSMState::when_udp(e);
	}

protected:
	p2 *p_;
};


/** �������ȴ���̨��λ...*/
class p2_starting: public FSMState
{
	p2 *p_;

public:
	p2_starting(p2 *p1)
		: FSMState(ST_P2_Staring, "starting")
	{
		p_ = p1;
	}

	virtual int when_timeout(double curr)
	{
		int x0 = atoi(kvc_get(p_->cfg(), "ptz_init_x", "0"));
		int y0 = atoi(kvc_get(p_->cfg(), "ptz_init_y", "0"));
		int z0 = atoi(kvc_get(p_->cfg(), "ptz_init_z", "5000"));

		ptz_setpos(p_->ptz(), x0, y0, 36, 36);	// ���ٹ�λ.
		ptz_setzoom(p_->ptz(), z0);	// ��ʼ����.

		p_->set_ptz_wait(ST_P2_Waiting, 2.0);

		return ST_P2_PtzWaiting;
	}
};


/** �ȴ���ִ̨�����״̬�������ϸ�״̬���ȴ������󣬷����ϸ�״̬ ..
  	�˶�ʱ���ڣ����������� ...*/
class p2_ptz_wait: public FSMState
{
	p2 *p_;

public:
	p2_ptz_wait(p2 *p1)
		: FSMState(ST_P2_PtzWaiting, "ptz wait")
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


/** ��̨�Ѿ���λ���ȴ�udp֪ͨ���� ...*/
class p2_waiting: public FSMState
{
	p2 *p_;

public:
	p2_waiting(p2 *p1)
		: FSMState(ST_P2_Waiting, "waiting udp start")
	{
		p_ = p1;
	}

	virtual int when_timeout(double curr) // ????? ....
	{
		return ST_P2_Searching;//Ϊ�˱��ڲ��ԣ��Ķ���;
	}

	// ���������������˳��¼�.
	virtual int when_udp(UdpEvent *e)
	{
		if (e->code() == UdpEvent::UDP_Start) {
			p_->set_ms(MS_TC); // ���ÿ���¼������ʾ����Ϊ��ʦ����...
		    p_->switch_ms();
			return ST_P2_Searching; 
		}
		else if (e->code() == UdpEvent::UDP_Quit) {
			return ST_P2_End; // ������������.
		}
		else {
			return id();
		}
	}
};


/** ��δ�ҵ�Ŀ�꣬�ȴ�̽���� ... */
class p2_searching: public p2_common_state
{
public:
	p2_searching(p2 *p1)
		: p2_common_state(p1, ST_P2_Searching, "searching target")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();

		if (targets.size() == 1) 
		{
			// ����Ŀ��.
			// set_pos ��Ŀ�꣬Ȼ��ȴ�ת����� ...
			int x, y;
			//�޷�����ת���Ƕȼ�������״̬...
			printf("searching:targets[0].x = %d, targets[0].width = %d&&&&&&\n", targets[0].x, targets[0].width);
			p_->calc_target_pos(targets[0], &x, &y);

			ptz_setpos(p_->ptz(), x, y, 20, 20);

			p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
			return ST_P2_Turnto_Target;
			
		}
		else 
		{
			//��Ŀ����̨��λ��ʱ��ʼ...
			p_->is_reset_ = false;
			p_->set_cam_reset(p_->reset_wait_);
			return ST_P2_No_Target;
		}
	}
};


/** ��ʦ��Ŀ��һ��ʱ����̨��λ ...*/
class p2_no_target:public p2_common_state
{
public:
	p2_no_target(p2 *p1)
		: p2_common_state(p1, ST_P2_No_Target, "teacher no target")
	{

	}
	virtual int when_detection(DetectionEvent *e)
	{
		std::vector<DetectionEvent::Rect> targets = e->targets();

		if(targets.size()  == 1)
		{
			return ST_P2_Searching;
		}
		else
		{
			// ��ʦ��Ŀ�곬ʱ��̨��λ��...
			if(!p_->is_reset_)
			{
				double curr = now();
				if(curr > p_->reset_back_)
				{
					int x0 = atoi(kvc_get(p_->cfg(), "ptz_init_x", "0"));
					int y0 = atoi(kvc_get(p_->cfg(), "ptz_init_y", "0"));
					int z0 = atoi(kvc_get(p_->cfg(), "ptz_init_z", "5000"));

					ptz_setpos(p_->ptz(), x0, y0, 36, 36);	// ���ٹ�λ.
					//ptz_setzoom(p_->ptz(), z0);	// ��ʼ����.
					p_->is_reset_ = true; // ��̨�ѹ�λ���´������ٴι�λ.
				}
			}		
			return id();
		}
	}
};


/** �ҵ�Ŀ�꣬ת��Ŀ�� */
class p2_turnto_target: public p2_common_state
{
	bool target_valid_;// Ŀ���Ƿ���Ч.
	DetectionEvent::Rect rc_;// �����Ч����ΪĿ��λ��.

public:
	p2_turnto_target(p2 *p1)
		: p2_common_state(p1, ST_P2_Turnto_Target, "turnto target")
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

		return id();// ���ﲻ�޸ĵ�ǰ״̬�����ǵ���̨��ɺ��ټ�� target_valid_
	}

	virtual int when_ptz_completed(PtzCompleteEvent *e)
	{
		// ��̨ת��λ��, ����ʱĿ���Ƿ�����Ұ��Χ�ڣ�������ڣ����������� ...
		double angle;
		if (!target_valid_) 
		{
			// Ŀ�궪ʧ ...
			ptz_stop(p_->ptz());//Ӧ���ǿɼӿɲ���...
			return ST_P2_Searching;
		}
		else if (p_->isin_field(rc_, angle)) 
		{
			printf("turn_to_targ(isin_field) **************\n");
			// Ŀ������Ұ�У������ȶ�����״̬ ...
			// ��ʱ�л�����ʦ����...
			//p_->set_ms(MS_TC); //�˴������л�������ģʽһֱ�ǽ�ʦ���� ????...
			//p_->switch_ms();
			return ST_P2_Tracking;
		}
		else 
		{
			printf("turn_to_targ(notin_field) **************\n");
			// Ŀ���Ѿ��뿪��Ұ���� set_pos ...
			int x, y;	
			// ����޷�����Ŀ��λ�ã�������������״̬???....
			p_->calc_target_pos(rc_, &x, &y);
			ptz_setpos(p_->ptz(), x, y, 20, 20);
			p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
			return ST_P2_Turnto_Target;
		}
	}
};


/** �ȶ�����״̬ */
class p2_tracking: public p2_common_state
{
public:
	p2_tracking(p2 *p1)
		: p2_common_state(p1, ST_P2_Tracking, "tracking")
	{
	}

	virtual int when_detection(DetectionEvent *e)
	{
		// ����̽��������ǰ��̨λ�ã��ж��Ƿ�Ŀ�궪ʧ ...
		double angle;
		std::vector<DetectionEvent::Rect> rcs = e->targets();
		if (rcs.size() != 1) 
		{
			printf("p1_tracking(no rcs) **************\n");
			ptz_stop(p_->ptz());
			return ST_P2_Searching;			
		}
		else 
		{
			// ������Ұ�У����� angle �����������ת ..
			int speed;
			if (p_->isin_field(rcs[0], angle)) 
			{
				//����̨����ʧ���޷���ȡת���ٶȸ÷����ĸ�״̬��???...
				if(!p_->ptz_speed(fabs(angle), speed))
				{
					return id();
				}
				if (speed == 0)
				{
					printf("p1_tracking(isin_field) speed=0 **************\n");
					ptz_stop(p_->ptz());
				}				
				else 
				{
					printf("p1_tracking(isin_field) angle=%f,speed=%d **************\n", angle, speed);
					if (angle < 0) ptz_left(p_->ptz(), speed);
					else ptz_right(p_->ptz(), speed);
				}
				return id();
			}
			else//Ŀ�겻����Ұ��... 
			{
				int x, y;				
				p_->calc_target_pos(rcs[0], &x, &y);
				ptz_setpos(p_->ptz(), x, y, 20, 20);

				p_->fsm()->push_event(new PtzCompleteEvent("teacher", "set_pos"));
				return ST_P2_Turnto_Target;
			}
		}
	}
};


/** ���� VGA ...*/
class p2_vga: public p2_common_state
{
public:
	p2_vga(p2 *p1)
		: p2_common_state(p1, ST_P2_Vga, "vga")
	{
	}

	//virtual int when_detection(DetectionEvent *e)
	//{
	//	// ����̽��������ǰ��̨λ�ã��ж��Ƿ�Ŀ�궪ʧ ...
	//	double angle;
	//	std::vector<DetectionEvent::Rect> rcs = e->targets();

	//	// �����Ŀ��������Ұ�� ms = MS_TC ; state = tracking״̬ ...
	//	if (rcs.size() == 1 && p_->isin_field(rcs[0], angle)) 
	//	{
	//		 p_->set_vga_last_state(ST_P2_Tracking);
	//	     
	//	}
	//	else // ���û��Ŀ�������Ұ��Χ�� ms = MS_SF  ;state = searching״̬ ...
	//	{
 //           p_->set_vga_last_state(ST_P2_Searching);
	//	
	//	}
	//	return id();
	//}

	virtual int when_timeout(double curr)
	{
		if (curr > p_->vga_back()) 
		{    
			// vga��ʱ���� �лؽ�ʦ����...	
			p_->set_ms(MS_TC);
			p_->switch_ms();
			return p_->get_vga_last_state();
		}
	}
};
