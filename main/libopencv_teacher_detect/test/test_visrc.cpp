#include <stdio.h>
#include <unistd.h>
#include "../vi_src.h"
#include "../hi_mat.h"
#include "../../vd/vd.h"

int main()
{
	vd_t *vd = vd_open();
	visrc_t *src = vs_open("teacher_detect_trace.config");
	size_t cnt = 0;
	
	while (1) {
		hiVIDEO_FRAME_INFO_S *vf;
		if (vs_next_raw(src, &vf)) {  // 这个就是得到 VI 的 VIDEO_FRAME_INFO_S
			if (vd_changed(vd, vf)) {
				fprintf(stderr, "C");
			}
			else {
				fprintf(stderr, ".");
			}
			vs_free_raw(src, vf);
		}
		else {
			fprintf(stderr, "ERR: no frame\n");
		}

		cnt++;
		if (cnt % 20 == 0) {
			fprintf(stderr, "\n");
		}

		usleep(100 * 1000);
	}
	vd_close(vd);
	vs_close(src);
	return 0;
}

