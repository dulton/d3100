#include <string.h>
#include "detect.h"
#include "../../hi353x/detect/mpi_wrap.h"
#include<pthread.h>
#include <string>	
#include <sstream>
#include <unistd.h>

typedef struct detect_t
{
	CHNS chns;

	TD td;	

	bool is_quit;
	pthread_t thread_id;
	pthread_mutex_t mutex;
	
	std::string s;
	POINT areas[8];
} detect_t;

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

void thread_delete(detect_t *det)
{
	int ret;
	while ( true != det->is_quit) {
		pthread_mutex_lock(&det->mutex);
		ret = read_hi3531(det->chns.vda_chn, &td);
		pthread_mutex_unlock(&det-<mutex);
		if (0 !=ret)
			exit("read hi3531 failed");

		usleep(200 * 1000);
	}
}

detect_t *det_open(const char *kvfname)
{
	int ret;
	SIZE size = {640, 560};
	detect_t *det = (detect_t *)malloc(sizeof(detect_t);

	det->vi_chn = 1;
	det->vda_chn = 1;
	det->is_quit = false;

	int ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
		comm_sys_exit();
	}
	
	ret = open_hi3531(det->chns, size, "./background.yuv");
	if (0 != ret)
		exit("open hi3531 failed!");
	pthread_create(det->thread_id, 0, thread_delete, det);
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
	close_hi3531(det->chns);
	free(det);
}
