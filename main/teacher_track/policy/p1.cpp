#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "p1.h"

p1::p1(const char *fname)
{
	kvc_ = kvc_open(fname);
	const char *ptz_url = kvc_get(kvc_, "ptz_serial_name", "tcp://172.16.1.110:10013/student");
	ptz_ = ptz_open(ptz_url);
	if (!ptz_) {
		fatal("p1", "can't open ptz of '%s'\n", ptz_url);
	}

	vga_wait_ = atof(kvc_get(kvc_, "vga_wait", "10.0"));
	ptz_wait_ = atof(kvc_get(kvc_, "ptz_wait", "2.0"));

	load_speeds(kvc_get(kvc_, "ptz_speeds", "0,1,3,6,10"), speeds_);

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
	if (!det_) {
		fatal("p1", "can't load detection mod!!!\n");
	}
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

void p1::load_speeds(const char *s, std::vector<int> &speeds)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, ',')) {
		speeds.push_back(std::atoi(item.c_str()));
	}

	std::sort(speeds.begin(), speeds.end());

	if (speeds.empty()) {
		speeds.push_back(0);
		speeds.push_back(1);
		speeds.push_back(3);
		speeds.push_back(6);
		speeds.push_back(10);
	}
}

