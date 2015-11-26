#pragma once
#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct detect_t detect_t;
struct hiVIDEO_FRAME_INFO_S;
detect_t *det_open(const char *kvfname);
void det_close(detect_t *det);

/*
	@param img: 图像.
	@return  ,json格式--   {"stamp":12345,"rect":[{"x":0,"y":0,"width":100,"height":100},
	                                              {"x":0,"y":0,"width":100,"height":100}]} 
	{
		"stamp":12345,
		"rect":
		[
			{"x":0,"y":0,"width":100,"height":100},
			{"x":0,"y":0,"width":100,"height":100}
		]
	}
 */
const char *det_detect(detect_t *det, struct hiVIDEO_FRAME_INFO_S *frame = 0);	// 返回json格式探测信息.

#ifdef __cplusplus
}
#endif // c++

