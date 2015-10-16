#include <stdio.h>
#include <stdlib.h>
#include "p1.h"

p1::p1(const char *fname)
{
	kvc_ = kvc_open(fname);
	ptz_ = ptz_open(kvc_get(kvc_, "ptz_serial_name", "tcp://172.16.1.110:10013/student"));

	vga_wait_ = atof(kvc_get(kvc_, "vga_wait", "10.0"));
	ptz_wait_ = atof(kvc_get(kvc_, "ptz_wait", "2.0"));

	// 构造状态转换表 ...
	std::vector<FSMState*> states;
	states.push_back(new p1_starting(this));
	states.push_back(new p1_waiting(this));
	states.push_back(new p1_vga(this));
	states.push_back(new p1_searching(this));
	states.push_back(new p1_turnto_target(this));
	states.push_back(new p1_tracking(this));
	states.push_back(new p1_ptz_wait(this));

	fsm_ = new FSM(states);
	udp_ = us_open(fsm_);
	det_ = detector_open(fsm_, fname);
}

p1::~p1()
{
	detector_close(det_);
	us_close(udp_);
	delete fsm_;
	kvc_close(kvc_);
	ptz_close(ptz_);
}

void p1::run()
{
	bool quit = false;
	fsm_->run(ST_P1_Staring, ST_P1_End, &quit);
}

