#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../ptz.h"

int main()
{
	/** 测试 ptz 模块 */
	
	const char *url = "tcp://127.0.0.1:10013/student";
	ptz_t *ptz = ptz_open(url);
	if (!ptz) {
		fprintf(stderr, "ERR: can't open url %s\n", url);
		return -1;
	}

	int x, y;
	if (ptz_getpos(ptz, &x, &y) < 0) {
		fprintf(stderr, "ERR: getpos err\n");
		return -1;
	}

	int z;
	if (ptz_getzoom(ptz, &z) < 0) {
		fprintf(stderr, "ERR: getzoom err\n");
		return -1;
	}

	if (ptz_setpos(ptz, 0, 0, 32, 32) < 0) {
		fprintf(stderr, "ERR: setpos errr\n");
		return -1;
	}

	if (ptz_setzoom(ptz, 0) < 0) {
		fprintf(stderr, "ERR: setzoom err\n");
		return -1;
	}

	if (ptz_left(ptz, 1) < 0) {
		fprintf(stderr, "ERR: left err\n");
		return -1;
	}

	usleep(3000 * 1000);

	if (ptz_stop(ptz) < 0) {
		fprintf(stderr, "ERR: stop err\n");
		return -1;
	}

	// 其他测试 ...

	ptz_close(ptz);

	fprintf(stdout, "test OK!\n");
	return 0;
}

