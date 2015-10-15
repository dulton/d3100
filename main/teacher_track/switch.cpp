#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "switch.h"

#define REC_PORT 8644

/** 使用udp通知到录播机 ... */
void ms_switch_to(MovieScene ms)
{
	const char *desc[] = {
		"",
		"TC (08 01)",
		"TF (08 02)",
		"SC (08 03)",
		"VGA(08 04)",
		"SF (08 05)",
		"BD (08 06)",
	};


	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (fd != -1) {
		char buf[2] = { 0x08, (unsigned char)ms};
		sockaddr_in to;
		to.sin_family = AF_INET;
		to.sin_port = htons(REC_PORT);
		to.sin_addr.s_addr = inet_addr("127.0.0.1");

		sendto(fd, buf, sizeof(buf), 0, (sockaddr*)&to, sizeof(to));

		closesocket(fd);
	}
}

