#include <stdio.h>
#include "../detect.h"
#include "../../libkvconfig/kvconfig.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <errno.h>

#define MULTICAST_ADDR "239.119.119.1"
#define PORT 11000

int main(void)
{
	struct sockaddr_in addr;
	int fd;
	int st;

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
	addr.sin_port = htons(PORT);


	const char *fname = NULL;
	kvconfig_t kvc = kvc_open(fname);
	if (!kvc) {
		fprintf(stderr, "can not get kv parameters\n");
		return -1;
	}

	detect_t *det = det_open(kvc);
	while (true) {
		char c;
		const char *message = det_detect(det);
	//	printf("###### message %s\n", message);
		std::string s(message);
		int len = s.size();
		if (s[len-3] == '[')
			continue;
		if ((st = sendto(fd, message, strlen(message), 0, (const sockaddr*)&addr, sizeof(addr))) < 0) {
			perror("sendto");
			printf("error %s\n", strerror(errno));
			usleep(100 * 1000);
		}
		usleep(100 * 1000);
	}
}
			
