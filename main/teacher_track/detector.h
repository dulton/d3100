#pragma once

#include "fsm.h"

typedef struct detector_t detector_t;

detector_t *detector_open(FSM *fsm, const char *cfgname);
void detector_close(detector_t *det);

