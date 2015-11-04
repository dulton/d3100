#include "config.h"
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

struct detect_t
{
	hi31_t *hi31;

	TD td;
	RECT rects[4];
	
	bool is_quit;
	pthread_t thread_id;
	pthread_mutex_t mutex;
	std::string s;
};

static void erase_str(std::string &s)
{
	int len = s.size();
	if (s[len-3] == ',')
		s.erase(len-3, 1);
}
static std::string get_aims_str(TD *td, RECT *rects)
{
	std::stringstream ss;
	int i = 0;
	int j = 0;
	int x;
	int y;
	int width;
	int height;

	int length = sizeof(td->is_alarms) /4 ;
	printf("length = %d\n", length);
	ss<<"{\"stamp\":"<<td->stamp<<",\"rect\":[";
	for (int k = 0; k < 4; k++)
		printf("$$$$$$ %d\n", td->is_alarms[i]);
	for (i = 0; i < length; i++) {
		if ((i < length - 2)&&(td->is_alarms[i]==1)&&(td->is_alarms[i+1]==1)&&(td->is_alarms[i+2]==1)) {
			for (j=i; j<i+3; j++) {
				ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
			} 	
			i = i + 2;
		}
			
		else if ((i < length -1) && (td->is_alarms[i]==1)&&(td->is_alarms[i+1]==1)) {
			x = (rects[i].x + rects[i+1].x) / 2;
			y = (rects[i].y + rects[i+1].y) / 2;
			width = (rects[i].width + rects[i+1].width) / 2;
			height = (rects[i].height + rects[i+1].height) / 2;
			ss<< "{\"x\":"<<x<<",\"y\":"<<y \
					<< ",\"width\":"<<width<<",\"height\":" \
					<<height<<"},";
			i = i + 1;
		}
		else if(td->is_alarms[i] == 1) {
			ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
		}
		else
			;
	}
	ss<<"]}";
	std::string s(ss.str());
	erase_str(s);
	return s;	
}

void *thread_proc(void *pdet)
{
	int ret;
	int i;
	detect_t *det = (detect_t*)pdet;	

	while (!det->is_quit) {
		TD td;
		ret = read_hi3531(det->hi31, &td);
		if (ret < 0) {
			usleep(10 * 1000);
			continue;
		}

#ifdef ZDEBUG
#endif // debug

		pthread_mutex_lock(&det->mutex);
		det->td = td;
#ifdef ZDEBUG
#endif // debug

		pthread_mutex_unlock(&det->mutex);

		usleep(200 * 1000);
	}

	return 0;
}

static void set_regions(RECT region, HI31_PS *ps)
{
	int i = 0;
	int x = region.x;
	int y = region.y;
	int width = region.width / 4;
	int height = region.height;


	for (i = 0; i < ps->vdas.num; i++) {
		ps->vdas.regions[i].rect.x = x + i * width;
		ps->vdas.regions[i].rect.y = y;
		ps->vdas.regions[i].rect.width = width;
		ps->vdas.regions[i].rect.height = height;
		ps->vdas.regions[i].st = 100;
		ps->vdas.regions[i].at = 45;
		ps->vdas.regions[i].ot = 1;
		ps->vdas.regions[i].ut = 0;
	}
}

detect_t *det_open(const char *kvfname)
{
	int ret;
	int x;
	int y;
	int width;
	int height;
	int i;
	RECT rect = {16, 17, 386, 32};
	HI31_PS ps;
	ps.vdas.size.width = 640;
	ps.vdas.size.height = 480;
	ps.vdas.num = 4;
	ps.chns.vi_chn = 16;
	ps.chns.vda_chn = 1;
	ps.vdas.image_file = "./background.yuv";	
	set_regions(rect, &ps);

	detect_t *det = new detect_t;
	det->is_quit = false;


	for (i=0; i++; i < ps.vdas.num) {
		det->rects[i] = ps.vdas.regions[i].rect;
	}

	ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
	}
	
	ret = open_hi3531(&det->hi31, ps);
	if (0 != ret) {
		PRT_ERR("err no 0x%x\n", ret);
	}
	pthread_create(&det->thread_id, 0, thread_proc, (void*)det);
	return det;
}

const char *det_detect(detect_t *det)
{
	detect_t detect;
	int len;
	
	pthread_mutex_lock(&det->mutex);
	det->s = get_aims_str(&det->td, det->rects);
	pthread_mutex_unlock(&det->mutex);

	return det->s.c_str();
}

void det_close(detect_t *det)
{
	det->is_quit = true;
	pthread_join(det->thread_id, 0);
	pthread_mutex_destroy(&det->mutex);
	close_hi3531(det->hi31);
	delete det;
}

