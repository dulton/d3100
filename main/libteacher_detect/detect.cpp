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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MULTICAST_ADDR "239.119.119.1"
#define PORT 11000

typedef struct detect_t
{
	hi31_t *hi31;

	TD td;
	RECT rects[4];
	
	bool is_quit;
	pthread_t thread_id;
	pthread_mutex_t mutex;
	std::string s;

	struct sockaddr_in addr;
	int addr_len;
	int fd;
} detect_t;

static std::string get_aims_str(TD *td, RECT *rects)
{
	std::stringstream ss;
	int i = 0;
	int j = 0;
	int x;
	int y;
	int width;
	int height;

	int length = sizeof(td->is_alarms);
	ss<<"{\"stamp\":"<<td->stamp<<",\"rect\":[";

	for (i = 0; i < length; i++) {
		if ((i < length - 2) && (td->is_alarms[i] * td->is_alarms[i+1] * td->is_alarms[i+2] == 1)) {
			for (j=0; j<3; j++) {
				ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
			} 	
			i = i + 2;
		}
			
		else if ((i < length -1) && (td->is_alarms[i] * td->is_alarms[i+1] == 1)) {
			x = (rects[i].x + rects[i+1].x) / 2;
			y = (rects[i].y + rects[i+1].y) / 2;
			width = (rects[i].width + rects[i+1].width) / 2;
			height = (rects[i].height + rects[i+1].height) / 2;
			ss<< "{\"x\":"<<x<<",\"y\":"<<y \
					<< ",\"width\":"<<width<<",\"height\":" \
					<<height<<"},";
			i = i + 1;
		}
		else if(td->is_alarms[i] == 1)
			ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
		else
			;
	}
	ss<<"]}";
	return ss.str();	
}

void *thread_delete(void *pdet)
{
	int ret;
	int i;
	detect_t *det = (detect_t*)pdet;	
	while ( true != det->is_quit) {
		pthread_mutex_lock(&det->mutex);
		ret = read_hi3531(det->hi31, &det->td);
		for (i = 0; i < 4; i++) {
			printf("%d\n", det->td.is_alarms[i]);
		}

		pthread_mutex_unlock(&det->mutex);
		if (0 !=ret)
			exit(-1);

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
	ps.chns.vi_chn = 28;
	ps.chns.vda_chn = 1;
	ps.vdas.image_file = "./background.yuv";	
	set_regions(rect, &ps);

	detect_t *det = (detect_t *)malloc(sizeof(detect_t));
	det->is_quit = false;

	if ((det->fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}
	
	det->addr_len = sizeof(det->addr);
	memset(&det->addr, 0, sizeof(det->addr));
	det->addr.sin_family = AF_INET;
	det->addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
	det->addr.sin_port = htons(PORT);

	for (i=0; i++; i < ps.vdas.num) {
		det->rects[i] = ps.vdas.regions[i].rect;
	}

	ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
		exit(-1);
	}
	
	ret = open_hi3531(&det->hi31, ps);
	if (0 != ret) {
		PRT_ERR("err no 0x%x\n", ret);
		exit(-1);
	}
	pthread_create(&det->thread_id, 0, thread_delete, (void*)det);
	return det;
}

const char *det_detect(detect_t *det)
{
	std::string s;
	detect_t detect;
	int len;
	
	pthread_mutex_lock(&det->mutex);
	det->s  = get_aims_str(&det->td, det->rects);
	pthread_mutex_unlock(&det->mutex);
	const char *message = det->s.c_str();
	if (sendto(det->fd, message, strlen(message), 0, (const sockaddr*)&det->addr, det->addr_len) < 0) {
		perror("sendto");
		exit(-1);
	}
	

	return det->s.c_str();
}

void det_close(detect_t *det)
{
	det->is_quit = true;
	pthread_join(det->thread_id, 0);
	pthread_mutex_destroy(&det->mutex);
	close_hi3531(det->hi31);
	free(det);
}
