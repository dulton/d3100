#include <string.h>
#include "detect.h"
#include "../../hi353x/detect/mpi_wrap.h"
#include<pthread.h>
#include <string>	
#include <sstream>

/* 获取目标 转换成 json字符串 */	
static std::string get_aims_str(bool *arms, point *centers, int stamp)
{
	int i;
	std::stringstream ss;
	std::string s;
	ss<<"{\"stamp\":"<<det->stamp<<",\"rect\":[";
	if (true == arms[0] && true == arms[1]) {
		ss<<"{\"x\":"<<centers[4].x<<"\"y\":"<< \
			centers[4].y<<",";
		arms[0] = false; arms[1] = false;
	}
	else if (true == arms[1] && true == arms[2]) {
		ss<<"{\"x\":"<<centers[5].x<<"\"y\":"<< \
			centers[5].y<<",";
		arms[1] = false; arms[2] = false;
	}
	else if (true == arms[2] && true == arms[3]) {
		ss<<"{\"x\":"<<centers[6].x<<"\"y\":"<< \
			centers[6].y<<",";
		arms[2] = false; arms[3] = false;
	}
	else if (true == arms[0]) {
		ss<<"{\"x\":"<<centers[0].x<<"\"y\":"<< \
			centers[0].y<<",";
	}
	else if (true == arms[1]) {
		ss<<"{\"x\":"<<centers[1].x<<"\"y\":"<< \
			centers[1].y<<",";
	}
	else if (true == arms[2]) {
		ss<<"{\"x\":"<<centers[2].x<<"\"y\":"<< \
			centers[2].y<<",";
	}
	else (true == arms[3]) {
		ss<<"{\"x\":"<<centers[3].x<<"\"y\":"<< \
			centers[3].y<<",";
	}
	// XXXX: 多一个,有影响否？
	ss<<"]}";
	return ss.str();
}

detect_t *det_open(const char *kvfname)
{
	int ret;
	SIZE size;
	kvconfig_t *kvc;
	detect_t *det = (detect_t *)malloc(sizeof(detect_t);
	VB_CONF_S vb_conf;

//	kvc = kvc_open(kvfname);

	det->vi_chn = 1;
	det->vda_chn = 1;
		
	ret = comm_sys_init();	
	if (0 != ret)
		comm_sys_exit();

	ret = comm_vi_memconfig(VI_MODE_4_1080P);
	if (0 != ret)
		comm_sys_exit();

	ret =  comm_vi_start(VI_MODE_4_1080P);	
	if (0 != ret)
		comm_sys_exit();
	

	size.width = 640;
	size.height = 480;
	ret = comm_vda_odstart(det->vda_chn, det->vi_chn,  &size,(detect_t*)det);
	if (0 != ret)
		comm_sys_exit();
}

const char *det_detect(detect_t *det)
{
	bool tmp[4];			
	int stamp;
	int i;
	pthread_mutex_lock(&det->mutex);
	for (i = 0; i < 4; i++)
		tmp[i] = det->is_arms[i];
	stamp = det->stamp;
	pthread_mutex_unlock(&det->mutex);
	for (i = 0; i < 4; i++) {
		if (true == tmp[i]) {
			int x = det->areas[i].x;
			int y = det->areas[i].y;
			int width = 0;
			int height = 0;
		}
	}
	det->s = get_aims_str(tmp, &det->centers, stamp);
	return det->s.c_str();
}

void det_close(detect *det)
{
	det->is_quit = true;
	pthread_join(det->thread_id);
	pthread_mutex_destroy(&det->mutex);
	comm_vda_odstop(det->vda_chn, det->vi_chn);
 	comm_vi_stop(VI_MODE_4_1080P);
	comm_sys_exit();
	free(det);
}
