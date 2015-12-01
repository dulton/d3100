#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vt.h"
#include "../libteacher_detect/detect.h"

#define PORT 11000 
#define IP "127.0.0.1"

struct vt_t
{
	int sock_f;
	sockaddr_in addr;
	
	detect_t *dt;
};

vt_t *vt_open(const char *fname) 
{
	vt_t *vt = new vt_t;
	if ((vt->sock_f = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "file: %s line: %d\n", __FILE__, __LINE__);
		perror("socket");
		exit(-1);
	}
	bzero(&vt->addr, sizeof(vt->addr));	
	vt->addr.sin_family = AF_INET;
	vt->addr.sin_port = htons(PORT);
	vt->addr.sin_addr.s_addr = inet_addr(IP);

	vt->dt = det_open(fname);

	return vt;
}

void vt_trace(vt_t *vt, int who, const hiVIDEO_FRAME_INFO_S *frame_480_270)
{
	char buff[200];		
	const char *s = det_detect_vt(vt->dt, frame_480_270);
	snprintf(buff, sizeof(buff), "%d%s", who, s);
	fprintf(stderr, "result: %s\n", s);
	sendto(vt->sock_f, buff, strlen(buff), 0, (struct sockaddr*)&vt->addr, sizeof(struct sockaddr_in));		
}

void vt_close(vt_t *vt)
{
	close(vt->sock_f);
	det_close(vt->dt);
	delete vt;
}

