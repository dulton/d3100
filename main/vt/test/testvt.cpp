#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../vt.h"
#include "../../libopencv_teacher_detect/vi_src.h"

int main()
{
	vt_t *vt = vt_open("teacher_detect_trace.config");
	visrc_t *vs = vs_open("./teacher_detect_trace.config");

	while (1) {
		hiVIDEO_FRAME_INFO_S *frame;
	
		if (vs_next_raw(vs, &frame)) {
			vt_trace(vt, 1, frame);
			vs_free_raw(vs, frame);
		}

		usleep(100 * 1000);
	}

	vt_close(vt);
	vs_close(vs);

	return 0;
}

