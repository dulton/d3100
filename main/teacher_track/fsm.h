#pragma once

/** 用于自动跟踪的fsm，根据策略实现 FSMState，状态变换由 FSMEvent 驱动

		目前预定义了 TimeoutEvent, PtzCompleteEvent, DetectionEvent, UdpEvent

		实现一个策略，就是实现各种状态的 FSMState ...
  	
 */

#include <vector>
#include <deque>
#include <pthread.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include "log.h"

class FSMEvent
{
public:
	FSMEvent(int id, const char *name);
	virtual ~FSMEvent() {}

	double stamp() const { return stamp_; }
	int id() const { return id_; }
	const char *name() { return name_.c_str(); }

	/// 每个事件实例的唯一值，用于取消事件 ...
	int token() const { return token_; }

protected:
	double stamp_;
	int id_, token_;
	std::string name_;
};


/** 声明已知的事件类型，
  	
  	TODO: 具体参数以后再加 ...
 */

// 超时事件
#define EVT_Timeout -1
class TimeoutEvent : public FSMEvent
{
public:
	TimeoutEvent(double wait): FSMEvent(EVT_Timeout, "EV_Timeout")
	{
		wait_ = wait;
	}

	double wait_;
};

// 云台完成事件
//    new PtzCompleteEvent("teacher", "setpos");
//	  ...
#define EVT_PTZ_Completed -2
class PtzCompleteEvent: public FSMEvent
{
	std::string who_, oper_;

public:
	PtzCompleteEvent(const char *who, const char *ptz_oper) 
		: FSMEvent(EVT_PTZ_Completed, "EV_PtzComplete")
	{
		who_ = who, oper_ = ptz_oper;
	}

	const char *who() const { return who_.c_str(); }
	const char *ptz_oper() const { return oper_.c_str(); }
};

// 探测结果事件
#define EVT_Detection -3
class DetectionEvent : public FSMEvent
{
public:
	/// 目标外接矩形 ...
	struct Rect
	{
		int x, y, width, height;
	};

	DetectionEvent(const char *who, const char *json_str)
		: FSMEvent(EVT_Detection, "EV_Detection")
	{
		who_ = who;	
		parse_json(json_str);
	}

	const char *who() const { return who_.c_str(); }
	std::vector<Rect> targets() const { return targets_; }

private:
	std::string who_; 	// "teacher", "student", "bd", ...
	std::vector<Rect> targets_;	// 解析后的目标 ...

private:
	void parse_json(const char *json_str);
};

// 结束事件
#define EVT_Quit -4
class QuitEvent: public FSMEvent
{
public:
	QuitEvent(): FSMEvent(EVT_Quit, "EV_Quit")
	{
	}
};

// UDP 命令
#define EVT_Udp -5
class UdpEvent: public FSMEvent
{
public:
	UdpEvent(int udpcode): FSMEvent(EVT_Udp, "EV_UDP")
	{
		udpcode_ = udpcode;
		if (udpcode_ == UDP_VGA) {
			token_ = -1;	// FIXME: VGA 通知仅仅最后一次触发时生效 ...
		}
	}

	int code() const { return udpcode_; }

	enum {
		UDP_Start,
		UDP_Stop,
		UDP_VGA,

		UDP_Quit,
	};

private:
	int udpcode_;
};


class FSMState
{
public:
	FSMState(int id, const char *name)
	{
		id_ = id, name_ = name;
	}

	int id() const { return id_; }
	const char *name() const { return name_.c_str(); }

	/** 当进入该状态时调用 */
	virtual void when_enter() { }

	/** 当离开改状态时调用 */
	virtual void when_leave() { }

	virtual int when_timeout(TimeoutEvent *evt) { return id_; }
	virtual int when_ptz_completed(PtzCompleteEvent *evt) { return id_; }
	virtual int when_detection(DetectionEvent *evt) { return id_; }
	virtual int when_udp(UdpEvent *udp) { return id_; }
	virtual int when_custom_event(FSMEvent *evt) { return id_; }

protected:
	int id_;
	std::string name_;
};


class FSM
{
public:
	FSM(const std::vector<FSMState*> &states)
		: states_(states)
	{
		pthread_mutex_init(&lock_, 0);
	}

	virtual ~FSM()
	{
		pthread_mutex_destroy(&lock_);
	}

	void run(int state_start, int state_end, bool *quit);

public:
	/** 由通知源调用，注意，将替换相同 token 的事件
	  		
	 */
	void push_event(FSMEvent *evt)
	{
		cancel_event(evt->token());

		if (evt->id() == EVT_Timeout) {
			push_timeout((TimeoutEvent*)evt);
			info("fsm", "push_event: Timeout, delay=%.3f\n", ((TimeoutEvent*)evt)->wait_);
		}
		else if (evt->id() == EVT_PTZ_Completed) {
			push_ptz_complete((PtzCompleteEvent*)evt);
			info("fsm", "push_event: Ptz, who=%s, op=%s\n",
					((PtzCompleteEvent*)evt)->who(), ((PtzCompleteEvent*)evt)->ptz_oper());

		}
		else if (evt->id() == EVT_Detection) {
			push_detection_result((DetectionEvent*)evt);
			//info("fsm", "push_event: Detection, cnt=%u\n",
			//		((DetectionEvent*)evt)->targets_.size());
		}
		else if (evt->id() == EVT_Udp) {
			push_udp_evt((UdpEvent*)evt);
			info("fsm", "push_event: Udp, code=%d\n",
					((UdpEvent*)evt)->code());
		}
		else {
			// TODO: custom event
		}
	}

	/** 取消已经投递的事件 */
	void cancel_event(int token);

protected:

private:
	std::vector<FSMState*> states_;	// 所有注册的状态 ...

	FSMState *find_state(int id) const
	{
		for (std::vector<FSMState*>::const_iterator it = states_.begin();
				it != states_.end(); ++it) {
			if ((*it)->id() == id)
				return *it;
		}
		return 0;
	}

	static bool op_sort_by_stamp(const std::pair<double, FSMEvent*> &e1, 
			const std::pair<double, FSMEvent*> &e2)
	{
		return e1.first < e2.first;
	}

	void push_timeout(TimeoutEvent *e)
	{
		pthread_mutex_lock(&lock_);
		double t = now() + e->wait_;	// 触发时间
		fifo_timeout_.push_back(std::pair<double, FSMEvent*>(t, e));
		std::sort(fifo_timeout_.begin(), fifo_timeout_.end(), op_sort_by_stamp);
		pthread_mutex_unlock(&lock_);
	}

	void push_detection_result(DetectionEvent *e)
	{
		/** XXX: 仅仅保留最后一个探测结果 ...
		 */
		pthread_mutex_lock(&lock_);
		while (!fifo_detection_.empty()) {
			FSMEvent *e = fifo_detection_.front();
			delete e;
			fifo_detection_.pop_front();
		}
		fifo_detection_.push_back(e);
		pthread_mutex_unlock(&lock_);
	}

	void push_ptz_complete(PtzCompleteEvent *e)
	{
		pthread_mutex_lock(&lock_);
		double t = now() + 2.0;		// FIXME: 云台总是使用2秒超时 ???
		fifo_ptz_complete_.push_back(std::pair<double, FSMEvent*>(t, e));
		std::sort(fifo_ptz_complete_.begin(), fifo_ptz_complete_.end(), op_sort_by_stamp);
		pthread_mutex_unlock(&lock_);
	}

	void push_udp_evt(UdpEvent *e)
	{
		pthread_mutex_lock(&lock_);
		fifo_udp_.push_back(e);
		pthread_mutex_unlock(&lock_);
	}

	/** 获取下一个事件，优先从 udp 队列中提取，然后检查超时完成，最后检查探测结果
	  	如果没有事件，则简单的睡上 50 ms
	 */
	FSMEvent *next_event(double curr)
	{
		FSMEvent *evt = 0;

		pthread_mutex_lock(&lock_);
		if (!fifo_udp_.empty()) {
			evt = fifo_udp_.front();
			fifo_udp_.pop_front();
		}
		else if (!fifo_timeout_.empty()) {
			if (curr >= fifo_timeout_.front().first) {
				evt = fifo_timeout_.front().second;
				fifo_timeout_.pop_front();
			}
		}
		else if (!fifo_ptz_complete_.empty()) {
			if (curr >= fifo_ptz_complete_.front().first) {
				evt = fifo_ptz_complete_.front().second;
				fifo_ptz_complete_.pop_front();
			}
		}
		else if (!fifo_detection_.empty()) {
			evt = fifo_detection_.front();
			fifo_detection_.pop_front();
		}
		pthread_mutex_unlock(&lock_);

		if (!evt)
			usleep(50*1000);

		return evt;
	}

	std::deque<std::pair<double, FSMEvent*> > fifo_timeout_;	// 时间相关的队列 ...
	std::deque<std::pair<double, FSMEvent*> > fifo_ptz_complete_;	// 云台完成
	std::deque<DetectionEvent*> fifo_detection_;	// 探测结果 ...
	std::deque<UdpEvent*> fifo_udp_;		// udp 通知 ...

	pthread_mutex_t lock_;

	static double now()
	{
		struct timeval tv;
		gettimeofday(&tv, 0);
		return tv.tv_sec + tv.tv_usec/1000000.0;
	}
};

