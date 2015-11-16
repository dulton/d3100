#pragma once

#ifndef _detect_h_
#define _detect_h_
#include<opencv2/opencv.hpp>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct det_t det_t;
typedef struct zifImage zifImage;
/** ����ʵ�� 
@param cfg_name: �����ļ�����
	@return det_tʵ����0 ʧ��
 */
det_t *det_open(const char *cfg_name);

void det_close(det_t *ctx);


/** 
	@param img: ͼ��
	@return  ,json��ʽ--   {"stamp":12345,"rect":[{"x":0,"y":0,"width":100,"height":100},{"x":0,"y":0,"width":100,"height":100}]}   





	{
		"stamp":12345,
		"rect":
		[
			{"x":0,"y":0,"width":100,"height":100},
			{"x":0,"y":0,"width":100,"height":100}
		]
	}
 */
//char *det_detect(det_t *ctx, zifImage *img);
char *det_detect(det_t *ctx, cv::Mat img);


#ifdef __cplusplus
};
#endif





#endif
