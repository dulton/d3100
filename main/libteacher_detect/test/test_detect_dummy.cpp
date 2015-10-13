#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../detect.h"

int main()
{
	detect_t *det = det_open("");
	if (!det) {
		fprintf(stderr, "ERR: can't open detector mod!\n");
		return -1;
	}

	while (1) {
		const char *result = det_detect(det);
		fprintf(stdout, "%s\n", result);

		usleep(100 * 1000);  // 模拟 10fps
	}

	det_close(det);

	return 0;
}

