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

// ...


#ifdef __cplusplus
}
#endif // c++

