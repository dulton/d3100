#pragma once

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

/** 提供运行时.
 */
#ifdef WIN32
#	include <WinSock2.h>
#	include <Ws2tcpip.h>

	typedef HANDLE pthread_t;
	typedef CRITICAL_SECTION pthread_mutex_t;
	typedef int socklen_t;

	int pthread_create(pthread_t *th, void *attr, void *(*proc)(void*), void *opaque);
	void pthread_join(pthread_t th, void **r);

	int pthread_mutex_init(pthread_mutex_t *m, void *attr);
	int pthread_mutex_destroy(pthread_mutex_t *m);
	int pthread_mutex_lock(pthread_mutex_t *m);
	int pthread_mutex_unlock(pthread_mutex_t *m);
	void usleep(int us);

#	define snprintf _snprintf
#	define __func__ __FUNCTION__


#else
#	include <unistd.h>
#	include <sys/socket.h>
#	include <sys/types.h>
#	include <arpa/inet.h>
#	include <fcntl.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <sys/time.h>
#	define closesocket close
#endif //

int set_sock_nonblock(int fd);
double now();

#include "switch.h"

