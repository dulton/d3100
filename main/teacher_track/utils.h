#pragma once

#include <pthread.h>

class Lock
{
public:
	Lock()
	{
		pthread_mutex_init(&lock_, 0);
	}

	~Lock()
	{
		pthread_mutex_destroy(&lock_);
	}

	void enter()
	{
		pthread_mutex_lock(&lock_);
	}

	void leave()
	{
		pthread_mutex_unlock(&lock_);
	}

private:
	pthread_mutex_t lock_;
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

