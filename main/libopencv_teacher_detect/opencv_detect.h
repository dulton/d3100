#pragma once


#ifdef __cplusplus
extern "C" {
#endif


typedef struct hi_detect_t hi_detect_t;

hi_detect_t* hi_det_open(const char *kvfname);

const char* hi_det_detect(hi_detect_t *hi_det);

void hi_det_colse(hi_detect_t *hi_det);

#ifdef __cplusplus
};
#endif