#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../ptz.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <tcp://.../who>\n", argv[0]);
		exit(-1);
	}

	/** 测试 ptz 模块 */
	
	const char *url = argv[1];

	ptz_t *ptz = ptz_open(url);
	if (!ptz) {
		fprintf(stderr, "ERR: can't open url %s\n", url);
		return -1;
	}
	fprintf(stderr, "INFO: open '%s' ok\n", url);

	int x, y;
	if (ptz_getpos(ptz, &x, &y) < 0) {
		fprintf(stderr, "ERR: getpos err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_pos: (%d,%d)\n", x, y);

	int z;
	if (ptz_getzoom(ptz, &z) < 0) {
		fprintf(stderr, "ERR: getzoom err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_zoom: (%d)\n", z);

	if (ptz_setzoom(ptz, 10000) < 0) {
		fprintf(stderr, "ERR: setzoom err\n");
		return -1;
	}
	fprintf(stderr, "INFO: set_zoom: 10000\n");
	usleep(2 * 1000 * 1000);

	if (ptz_getzoom(ptz, &z) < 0) {
		fprintf(stderr, "ERR: getzoom err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_zoom: (%d)\n", z);

	fprintf(stderr, "INFO: set_pos: %d,%d\n", 200, -200);
	if (ptz_setpos(ptz, 200, -200, 32, 32) < 0) {
		fprintf(stderr, "ERR: setpos errr\n");
		return -1;
	}
	usleep(2 * 1000 * 1000);

	if (ptz_getpos(ptz, &x, &y) < 0) {
		fprintf(stderr, "ERR: getpos err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_pos: (%d,%d)\n", x, y);

	fprintf(stderr, "left ...\n");
	if (ptz_left(ptz, 3) < 0) {
		fprintf(stderr, "ERR: left err\n");
		return -1;
	}
	usleep(3 * 1000 * 1000);

	fprintf(stderr, "stop ..\n");
	if (ptz_stop(ptz) < 0) {
		fprintf(stderr, "ERR: stop err\n");
		return -1;
	}

	if (ptz_getpos(ptz, &x, &y) < 0) {
		fprintf(stderr, "ERR: getpos err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_pos: (%d,%d)\n", x, y);

	fprintf(stderr, "INFO: zoom wide ...\n");
	if (ptz_zoom_wide(ptz, 1) < 0) {
		fprintf(stderr, "ERR: zoom_wide err\n");
		return -1;
	}
	usleep(1 * 1000 * 1000);
	
	fprintf(stderr, "INFO: zoom stop...\n");
	if (ptz_zoom_stop(ptz) < 0) {
		fprintf(stderr, "ERR: zoom_stop err\n");
		return -1;
	}

	if (ptz_getzoom(ptz, &z) < 0) {
		fprintf(stderr, "ERR: getzoom err\n");
		return -1;
	}
	fprintf(stderr, "INFO: get_zoom: (%d)\n", z);

	// 其他测试 ...


	// 归位
	ptz_setzoom(ptz, 0);
	ptz_setpos(ptz, 0, 0, 32, 32);

	ptz_close(ptz);

	fprintf(stdout, "test OK!\n");
	return 0;
}

