#pragma once

#include <sys/time.h>

inline double uty_now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec * 0.000001;
}

inline double uty_uptime()
{
	static double begin_ = uty_now();
	return uty_now() - begin_;
}

class UtyTimeUsed
{
	double delay_, begin_;
	const char *prev_;	// FIXME: 没有存储 ...

public:
	/// 默认超过 50ms 就应该提示 ...
	UtyTimeUsed(const char *prev = "no prev", double delay = 0.050)
	{
		begin_ = uty_now();
		prev_ = prev;
		delay_ = delay;
	}

	~UtyTimeUsed()
	{
		double now = uty_now();
		if (now - begin_ > delay_) {
			fprintf(stderr, "DEBUG: UTU: %s: timeout, using %.3f seconds\n",
					prev_, now - begin_);
		}
	}
};

