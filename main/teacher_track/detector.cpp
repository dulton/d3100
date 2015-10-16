#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "detector.h"
#include "../libteacher_detect/detect.h"

struct detector_t
{
	int quit;
	pthread_t th;
	FSM *fsm;
	detect_t *detimpl;	// 真正的实现...
};

static void parse_and_handle(FSM *fsm, const char *str)
{
	FSMEvent *e = new DetectionEvent("teacher", str);
	
	fsm->push_event(e);
}

static void *thread_proc(void *arg)
{
	detector_t *p = (detector_t*)arg;

	int cnt = 0;

	while (!p->quit) {
		const char *result = det_detect(p->detimpl);
		if (result) {
			parse_and_handle(p->fsm, result);
		}

		usleep(1000*1000);	// FIXME: 将无法保证 ...
	}
	return 0;
}

detector_t *detector_open(FSM *fsm, const char *fname)
{
	detector_t *det = new detector_t;
	if (!det) {
		fatal("detector", "new err ??\n");
		return 0;
	}
	det->detimpl = det_open(fname);
	if (!det->detimpl) {
		fatal("detector", "det_open err\n");
		delete det;
		return 0;
	}

	det->fsm = fsm;
	det->quit = 0;

	if (0 != pthread_create(&det->th, 0, thread_proc, det)) {
		fatal("detect", "can't create detection thread..\n");
		delete det;
		return 0;
	}

	return det;
}

void detector_close(detector_t *det)
{
	det->quit = true;
	void *p;
	pthread_join(det->th, &p);
	det_close(det->detimpl);
	delete det;
}

