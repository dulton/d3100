#include <stdio.h>
#include <unistd.h>
#include "../vi_src.h"
#include "../hi_mat.h"

int main()
{
	visrc_t *src = vs_open("teacher_detect_trace.config");
	while (1) {
		hiMat m;
		if (vs_next(src, m)) {
			m.dump_hdr();
		}
		else {
			fprintf(stderr, "ERR: no frame\n");
		}

		usleep(100 * 1000);
	}
	vs_close(src);
	return 0;
}

