#include "runtime.h"

#ifdef WIN32
#include <process.h>

struct ThreadStartParam
{
	void *(*proc)(void*);
	void *opaque;
};

static unsigned int __stdcall thread_proc(void *arg)
{
	ThreadStartParam *tsp = (ThreadStartParam*)arg;
	return 0;
}

int pthread_create(pthread_t *th, void *attr, void *(*proc)(void*), void *opaque)
{
	ThreadStartParam *tsp = new ThreadStartParam;
	tsp->proc = proc, tsp->opaque = opaque;

	*th = (HANDLE)_beginthreadex(0, 0, thread_proc, tsp, 0, 0);
	return 0;
}

void pthread_join(pthread_t th, void **r)
{
	WaitForSingleObject(th, -1);
	CloseHandle(th);
	*r = 0;
}

int pthread_mutex_init(pthread_mutex_t *m, void *attr)
{
	InitializeCriticalSection(m);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m)
{
	DeleteCriticalSection(m);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
	EnterCriticalSection(m);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
	LeaveCriticalSection(m);
	return 0;
}

void usleep(int us)
{
	Sleep(us/1000);
}

class winsock_init
{
public:
	winsock_init()
	{
		WSAData data;
		WSAStartup(0x202, &data);
	}
};
static winsock_init _sock_init;

#endif //

int set_sock_nonblock(int fd)
{
#ifdef WIN32
	unsigned long mode = 1;
	return ioctlsocket(fd, FIONBIO, &mode);
#else
	return fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0));
#endif //
}


double now()
{
#ifdef WIN32
	return GetTickCount()/1000.0;
#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
#endif
}
