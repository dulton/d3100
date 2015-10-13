#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "fsm.h"

static double util_now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

FSMEvent::FSMEvent(int id)
	: id_(id)
{
	stamp_ = util_now();
}

