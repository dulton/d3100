#pragma once

#include "runtime.h"
#include "log.h"

class Lock
{
public:
	Lock(const char *name = 0)
	{
		pthread_mutex_init(&lock_, 0);
		if (name) name_ = name;
		else {
			char buf[16];
			snprintf(buf, sizeof(buf), "%p", this);
			name_ = buf;
		}
	}

	~Lock()
	{
		pthread_mutex_destroy(&lock_);
	}

	void enter()
	{
#ifdef ZDEBUG_LOCK
		debug("lock", "before lock %s\n", name_.c_str());
#endif
		pthread_mutex_lock(&lock_);
	}

	void leave()
	{
		pthread_mutex_unlock(&lock_);
#ifdef ZDEBUG_LOCK
		debug("unlock", "release lock %s\n", name_.c_str());
#endif
	}

private:
	pthread_mutex_t lock_;
	std::string name_;
};

class Autolock
{
public:
	Autolock(Lock &l): lock_(l)
	{
		lock_.enter();
	}

	~Autolock()
	{
		lock_.leave();
	}

private:
	Lock &lock_;
};

