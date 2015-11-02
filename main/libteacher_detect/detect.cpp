#include <stdlib.h>
#include <string.h>
#include "detect.h"
#include "../../hi353x/detect/mpi_wrap.h"
#include <pthread.h>
#include <string>	
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

typedef struct detect_t
{
	CHNS chns;
	TD td;	

	bool is_quit;
	pthread_t thread_id;
	pthread_mutex_t mutex;
	
	std::string s;
	POINT points[7];
} detect_t;

/* 获取目标 转换成 json字符串 */	
static std::string get_aims_str(int *arms, POINT *centers, int stamp)
{
	int i;
	std::stringstream ss;
	std::string s;
	ss<<"{\"stamp\":"<<stamp<<",\"rect\":[";
	if (1 == arms[0] && 1 == arms[1]) {
		ss<<"{\"x\":"<<centers[4].x<<"\"y\":"<< \
			centers[4].y<<",";
		arms[0] = false; arms[1] = false;
	}
	else if (1 == arms[1] && 1 == arms[2]) {
		ss<<"{\"x\":"<<centers[5].x<<"\"y\":"<< \
			centers[5].y<<",";
		arms[1] = false; arms[2] = false;
	}
	else if (1 == arms[2] && 1 == arms[3]) {
		ss<<"{\"x\":"<<centers[6].x<<"\"y\":"<< \
			centers[6].y<<",";
		arms[2] = false; arms[3] = false;
	}
	else if (1 == arms[0]) {
		ss<<"{\"x\":"<<centers[0].x<<"\"y\":"<< \
			centers[0].y<<",";
	}
	else if (1 == arms[1]) {
		ss<<"{\"x\":"<<centers[1].x<<"\"y\":"<< \
			centers[1].y<<",";
	}
	else if (1 == arms[2]) {
		ss<<"{\"x\":"<<centers[2].x<<"\"y\":"<< \
			centers[2].y<<",";
	}
	else if (1 == arms[3]) {
		ss<<"{\"x\":"<<centers[3].x<<"\"y\":"<< \
			centers[3].y<<",";
	}
	else {
		ss<<"{\"x\":"<<-1<<"\"y\":"<< \
			-1<<",";
	}
	// XXXX: 多一个,有影响否？
	ss<<"]}";
	return ss.str();
}

void *thread_delete(void *pdet)
{
	int ret;
	detect_t *det = (detect_t*)pdet;	
	while ( true != det->is_quit) {
		pthread_mutex_lock(&det->mutex);
		ret = read_hi3531(det->chns.vda_chn, &det->td);
		pthread_mutex_unlock(&det->mutex);
		if (0 !=ret)
			exit(-1);

		usleep(200 * 1000);
	}
	return 0;
}

detect_t *det_open(const char *kvfname)
{
	int ret;
	int x;
	int y;
	int width;
	int height;
	int i;

	SIZE size = {640, 560};
	detect_t *det = (detect_t *)malloc(sizeof(detect_t));

	det->chns.vi_chn = 12;
	det->chns.vda_chn = 1;
	det->is_quit = false;

	ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
		exit(-1);
	}
	VDAS vdas = {4, 100, 45, 1, 0, {480, 270}, {14, 71, 440, 28}, "./background.yuv"};	
	width = vdas.rect.width / 2;
	height = vdas.rect.height / 2;
	x = vdas.rect.x + width;
	y = vdas.rect.y + height;
	for (i = 0; i < 7; i++) {
		det->points[i].x = x + i * width;
		det->points[i].y = y;
	}
	ret = open_hi3531(det->chns, vdas);
	if (0 != ret)
		exit(-1);
	pthread_create(&det->thread_id, 0, thread_delete, (void*)det);
	return det;
}

const char *det_detect(detect_t *det)
{
	int tmp[4];			
	int stamp;
	int i;
	pthread_mutex_lock(&det->mutex);
	for (i = 0; i < 4; i++)
		tmp[i] = det->td.is_alarms[i];
	stamp = det->td.stamp;
	pthread_mutex_unlock(&det->mutex);
	for (i = 0; i < 4; i++) {
		if (1 == tmp[i]) {
			int x = det->points[i].x;
			int y = det->points[i].y;
		}
	}
	det->s = get_aims_str(tmp, (POINT*)&det->points, stamp);
	return det->s.c_str();
}

void det_close(detect_t *det)
{
	det->is_quit = true;
	pthread_join(det->thread_id, 0);
	pthread_mutex_destroy(&det->mutex);
	close_hi3531(det->chns);
	free(det);
}
