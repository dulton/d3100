
#include <string.h>
#include <unistd.h>
#include <sys/type.h>
#include <sys/socket>
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

vt_t *vt_open() 
{
	vt = new vt_t;
	if ((vt->sock_f = sock(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "file: %s line: %s\n", __FILE__, __LINE__);
		perror("socket");
		exit(-1);
	}
	bzero(vt->addr);	
	vt->addr.sin_family = AF_INET;
	vt->addr.sin_port = htons(PORT);
	vt->addr.sin_addr.s_addr = inet_addr(IP);

	vt->dt = det_open(0);
}

void vt_trace(vt_t *vt, const char *who, const hiVIDEO_FRAME_INFO_S *frame_480_270)
{
	char buff[200];		
	memset(buff, 0, sizeof(buff));
	const char *s = det_detect(vt_t->dt, frame_480_270);
	snprintf(buff, 200 - 1, "%d%s", who, s);
	sendto(vt->sock_f, buff, strlen(buff) + 1, 0, (struct sockaddr*)&vt->addr, sizeof(struct sockaddr_in));		
}

void vt_close(vt_t *vt)
{
	close(vt->sock);
	det_close(vt->dt);
	detete vt;
}

