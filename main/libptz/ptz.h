#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct ptz_t ptz_t;

ptz_t *ptz_open(const char *url);
void ptz_close(ptz_t *ptz);

int ptz_left(ptz_t *ptz, int speed);
int ptz_right(ptz_t *ptz, int speed);
int ptz_up(ptz_t *ptz, int speed);
int ptz_down(ptz_t *ptz, int speed);
int ptz_stop(ptz_t *ptz);

int ptz_setpos(ptz_t *ptz, int x, int y, int sx, int sy);
int ptz_getpos(ptz_t *ptz, int *x, int *y);

int ptz_setzoom(ptz_t *ptz, int z);
int ptz_getzoom(ptz_t *ptz, int *z);

int ptz_zoom_tele(ptz_t *ptz, int speed);
int ptz_zoom_wide(ptz_t *ptz, int speed);
int ptz_zoom_stop(ptz_t *ptz);

int ptz_preset_clr(ptz_t *ptz, int id);
int ptz_preset_save(ptz_t *ptz, int id);
int ptz_preset_call(ptz_t *ptz, int id);

/// x,y 为鼠标点击位置，在图像的相对位置 [0.0, 1.0)
int ptz_ext_mouse_track(ptz_t *ptz, float x, float y);

// ...


#ifdef __cplusplus
}
#endif // c++

