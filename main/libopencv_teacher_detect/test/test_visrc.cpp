#include <stdio.h>
#include <unistd.h>
#include "../vi_src.h"
#include "../hi_mat.h"
#include "../../vd/vd.h"

static vd_t *VD_open()
{
	// TODO: return vd_open();
	return 0;
}

static int VD_changed(vd_t *vd, hiVIDEO_FRAME_INFO_S *f)
{
	// TODO: return vd_changed(vd, f);
	return 0;
}

int main()
{
	vd_t *vd = VD_open();
	visrc_t *src = vs_open("teacher_detect_trace.config");
	while (1) {
		hiVIDEO_FRAME_INFO_S *vf;
		if (vs_next_raw(src, &vf)) {
			if (VD_changed(vd, vf)) {
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

#if 0
		hiMat m;
		if (vs_next(src, m)) {
			m.dump_hdr();
		}
		else {
			fprintf(stderr, "ERR: no frame\n");
		}
#endif // 
		usleep(100 * 1000);
	}
	vd_close(vd);
	vs_close(src);
	return 0;
}

