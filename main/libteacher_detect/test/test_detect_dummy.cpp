#include <stdio.h>
#include <stdlib.h>
#include "../../teacher_track/runtime.h"
#include "../detect.h"

static void *thread_proc(void *p)
{
	fprintf(stderr, "thread mode...\n");

	detect_t *det = (detect_t*)p;

	while (1) {
		const char *result = det_detect(det);
		fprintf(stdout, "%s\n", result);

		usleep(100 * 1000);  // 模拟 10fps
	}
	
	return 0;
}

int main()
{
	detect_t *det = det_open("");
	if (!det) {
		fprintf(stderr, "ERR: can't open detector mod!\n");
		return -1;
	}

#if 1
	while (1) {
		const char *result = det_detect(det);
		fprintf(stdout, "%s\n", result);

		usleep(100 * 1000);  // 模拟 10fps
	}
#else
	pthread_t th;
	pthread_create(&th, 0, thread_proc, det);
	usleep(30 * 1000 * 1000);
#endif // 

	det_close(det);

	return 0;
}

