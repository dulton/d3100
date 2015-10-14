#pragma once

/** 一般每个策略需要实现自己的 FSMEvent, FSMTransition, FSM
 */

#include <vector>
#include <deque>
#include <pthread.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>

class FSMEvent
{
public:
	FSMEvent(int id);
	virtual ~FSMEvent() {}

	double stamp() const { return stamp_; }
	int id() const { return id_; }

protected:
	double stamp_;
	int id_;
};


/** 声明已知的事件类型，
  	
  	TODO: 具体参数以后再加 ...
 */

// 超时事件
#define EVT_Timeout -1
class TimeoutEvent : public FSMEvent
{
public:
	TimeoutEvent(double wait): FSMEvent(EVT_Timeout)
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
	PtzCompleteEvent(const char *who, const char *ptz_oper) : FSMEvent(EVT_PTZ_Completed)
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

	DetectionEvent(const char *who, const char *json_str): FSMEvent(EVT_Detection)
	{
		who_ = who;	
		parse_json(json_str);
	}

	std::string who_; 	// "teacher", "student", "bd", ...
	std::vector<Rect> targets_;	// 解析后的目标 ...
	double stamp_;		// 来自json

private:
	void parse_json(const char *json_str);
};

// 结束事件
#define EVT_Quit -4
class QuitEvent: public FSMEvent
{
public:
	QuitEvent(): FSMEvent(EVT_Quit)
	{
	}
};

// UDP 命令
#define EVT_Udp -5
class UdpEvent: public FSMEvent
{
public:
	UdpEvent(int udpcode): FSMEvent(EVT_Udp)
	{
		udpcode_ = udpcode;
	}

	int udpcode_;

	enum {
		UDP_Start,
		UDP_Stop,
		UDP_VGA,
	};
};

class FSMTransition
{
public:
	FSMTransition(int state, int evt_id): state_(state), eid_(evt_id) {}
	virtual ~FSMTransition() {}

	/// 必须实现的转换 ...
	virtual int operator()(void *opaque, FSMEvent *evt) = 0;

	int state() const { return state_; }
	int event_id() const { return eid_; }

protected:
	int state_, eid_;
};


class FSM
{
public:
	FSM(const std::vector<FSMTransition*> &trans, void *opaque)
		: trans_(trans)
		, opaque_(opaque)
	{
		pthread_mutex_init(&lock_, 0);
	}

	virtual ~FSM()
	{
		pthread_mutex_destroy(&lock_);
	}

	void run(int state_start, int state_end, void (*first_oper)(void *opaque))
	{
		int curr_state = state_start;
		if (first_oper) first_oper(opaque_);
		while (curr_state != state_end) {
			FSMEvent *evt = next_event(now());
			if (evt) {
				// !evt 说明此段时间内，没有事件 ...
				FSMTransition *trans = find_trans(curr_state, evt->id());
				if (trans)
					curr_state = (*trans)(opaque_, evt);

				delete evt;	// XXX:
			}
		}
	}

public:
	/** 由通知源调用
	  		
	 */
	void push_event(FSMEvent *evt)
	{
		if (evt->id() == EVT_Timeout) {
			push_timeout((TimeoutEvent*)evt);
		}
		else if (evt->id() == EVT_PTZ_Completed) {
			push_ptz_complete((PtzCompleteEvent*)evt);
		}
		else if (evt->id() == EVT_Detection) {
			push_detection_result((DetectionEvent*)evt);
		}
		else if (evt->id() == EVT_Udp) {
			push_udp_evt((UdpEvent*)evt);
		}
		else {
			push_unknown_event(evt);
		}
	}

protected:
	virtual void push_unknown_event(FSMEvent *evt)
	{
	}

protected:
	std::vector<FSMTransition*> trans_;
	void *opaque_;

private:
	FSMTransition *find_trans(int curr_state, int curr_evid) const
	{
		std::vector<FSMTransition*>::const_iterator it;
		for (it = trans_.begin(); it != trans_.end(); ++it) {
			if ((*it)->state() == curr_state && (*it)->event_id() == curr_evid)
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

