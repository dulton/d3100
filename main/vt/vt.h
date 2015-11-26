#ifndef	__VT_H__
#define __VT_H__
#ifdef __cplusplus
extern "C" {
#endif

struct hiVIDEO_FRAME_INFO_S; // 需要外部包含 hixxx.h 

typedef struct vt_t vt_t;

vt_t *vt_open(const char *cfg_fname);
void vt_trace(vt_t *vt, int who, const hiVIDEO_FRAME_INFO_S *frame_480_270);
void vt_close(vt_t *vt);

#ifdef __cplusplus
}
#endif
#endif
