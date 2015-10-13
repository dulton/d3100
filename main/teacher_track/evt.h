#pragma once

#include "fsm.h"

/** 声明已知的事件类型，
  	
  	TODO: 具体参数以后再加 ...
 */

// 超时事件
#define EVT_Timeout -1
class TimeoutEvent : public FSMEvent
{
public:
	TimeoutEvent() : FSMEvent(EVT_Timeout)
	{
	}
};

// 云台完成事件
#define EVT_PTZ_Completed -2
class PtzCompleteEvent: public FSMEvent
{
public:
	PtzCompleteEvent() : FSMEvent(EVT_PTZ_Completed)
	{
	}
};

// 探测结果事件
#define EVT_Detection -3
class DetectionEvent : public FSMEvent
{
public:
	DetectionEvent(): FSMEvent(EVT_Detection)
	{
	}
};

// 结束事件
#define EVT_Quit -4
class QuitEvent: public FSMEvent
{
public:
	QuitEvent(): FSMEvent(EVT_Quit)
	{
	}
};

// UDP 命令
#define EVT_Udp -5
class UdpEvent: public FSMEvent
{
public:
	UdpEvent(int udpcode): FSMEvent(EVT_Udp)
	{
		udpcode_ = udpcode;
	}

	int udpcode_;
};

