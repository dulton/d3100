#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct ptz_t ptz_t;

ptz_t *ptz_open(const char *url);
void ptz_close(ptz_t *ptz);

// ...


#ifdef __cplusplus
}
#endif // c++

