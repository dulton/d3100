#pragma once

#include "fsm.h"

typedef struct udpsrv_t udpsrv_t;

udpsrv_t *us_open(FSM *fsm, int port = 8642, const char *bindip = "0.0.0.0");
void us_close(udpsrv_t *udp);

