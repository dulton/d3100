#include <stdio.h>
#include <stdlib.h>
#include "p1.h"

p1::p1(const char *fname)
{
	kvc_ = kvc_open(fname);
	ptz_ = ptz_open(kvc_get(kvc_, "ptz_serial_name", "tcp://127.0.0.1:1001"));
	det_ = det_open(fname);

	// 构造状态转换表 ...
	std::vector<FSMTransition*> trans;
	trans.push_back(new p1_starting_timeout);
	trans.push_back(new p1_inited_search_target);
	trans.push_back(new p1_inited_recv_udp);

	// 结束转换
	trans.push_back(new p1_quit(ST_P1_Staring));
	trans.push_back(new p1_quit(ST_P1_Inited));

	fsm_ = new FSM(trans, this);
}

p1::~p1()
{
	delete fsm_;
	kvc_close(kvc_);
	ptz_close(ptz_);
	det_close(det_);
}

void p1::boot()
{
	// TODO: 云台归位，并且投递一个2秒延迟的超时事件 ...
}

void p1::run()
{
	fsm_->run(ST_P1_Staring, ST_P1_End, boot);
}

