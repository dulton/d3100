#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct detect_t detect_t;

detect_t *det_open(const char *kvfname);
void det_close(detect_t *det);

const char *det_detect(detect_t *det);	// 返回json格式探测信息

#ifdef __cplusplus
}
#endif // c++

