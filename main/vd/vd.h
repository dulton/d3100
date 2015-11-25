#ifndef	__VD_H__
#define __VD_H__
#ifdef __cplusplus
extern "C" {
#endif

struct hiVIDEO_FRAME_INFO_S; // 需要外部包含 hixxx.h 

typedef struct vd_t vd_t;
vd_t *vd_open();
int vd_changed(vd_t *vd, const hiVIDEO_FRAME_INFO_S *curr_vga_frame);
void vd_close(vd_t *vd);

#ifdef __cplusplus
}
#endif
#endif

