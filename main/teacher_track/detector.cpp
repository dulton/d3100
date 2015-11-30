#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "detector.h"
#include "../libteacher_detect/detect.h"
#include "../libopencv_teacher_detect/utils.h"
struct detector_t
{
	int quit;
	pthread_t th;
	FSM *fsm;
	detect_t *detimpl;	// 真正的实现...
};

static void parse_and_handle(FSM *fsm, int who, const char *str)
{
	FSMEvent *e = 0;
	if (who == 1) {
		e = new DetectionEvent("teacher", str);
	}
	
	fsm->push_event(e);
}

static void *thread_proc(void *arg)
{
	detector_t *p = (detector_t*)arg;

	int cnt = 0;
	size_t n = 0;
	double begin = uty_now(), end;
	double delta = 0.0;
	double def_wait = 0.1;
	double frame_delay = 0.0;
	double fr = .0;

	while (!p->quit) {
		const char *result = det_detect(p->detimpl, 0);
		if (result) {
			parse_and_handle(p->fsm, 1, result);
		}

		n++;
		if (n % 100 == 0) {
			end = uty_now();

			frame_delay = (end - begin) / n;	// 这是每帧实际消耗时间的平均值，
			delta = frame_delay - def_wait;
			fr = 1/frame_delay;
		}

		if (delta > def_wait) delta = def_wait - 0.0000001;
		usleep((def_wait - delta) * 1000000);
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

