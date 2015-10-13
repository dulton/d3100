#pragma once

/** 一般每个策略需要实现自己的 FSMEvent, FSMTransition, FSM
 */

#include <vector>
#include <deque>
#include <pthread.h>

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
		pthread_cond_init(&cond_, 0);
	}

	virtual ~FSM()
	{
		pthread_cond_destroy(&cond_);
		pthread_mutex_destroy(&lock_);
	}

	void run(int state_start, int state_end, void (*first_oper)(void *opaque))
	{
		int curr_state = state_start;
		if (first_oper) first_oper(opaque_);
		while (curr_state != state_end) {
			FSMEvent *evt = next_event();
			FSMTransition *trans = find_trans(curr_state, evt->id());
			if (trans)
				curr_state = (*trans)(opaque_, evt);
		}
	}

protected:
	void push_event(FSMEvent *evt)
	{
		pthread_mutex_lock(&lock_);
		evt_fifo_.push_back(evt);
		pthread_cond_signal(&cond_);
		pthread_mutex_unlock(&lock_);
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

	FSMEvent *next_event()
	{
		pthread_mutex_lock(&lock_);
		if (evt_fifo_.empty()) {
			pthread_cond_wait(&cond_, &lock_);
		}

		FSMEvent *evt = evt_fifo_.front();
		evt_fifo_.pop_front();
		pthread_mutex_unlock(&lock_);

		return evt;
	}

	std::deque<FSMEvent*> evt_fifo_;
	pthread_mutex_t lock_;
	pthread_cond_t cond_;
};

