#include <stdio.h>
#include <stdlib.h>
#include "../udpsrv.h"
#include <unistd.h>

int main()
{
	udpsrv_t *us = us_open(0, 8642, "0.0.0.0");
	for (int i = 0; i < 120; i++) { // 两分钟 ...
		usleep(1000 * 1000);
	}
	us_close(us);

	fprintf(stdout, "OK\n");
	return 0;
}

