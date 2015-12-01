#include "policy.h"
#include "reg.h"

// find_policy() 的实现放在 reg.cpp 中.

Policy::Policy(const char*filename)
{
	kvc_ = kvc_open(filename);
	current_policy_ = kvc_get(kvc_, "current_policy", "sp1");

}

Policy::~Policy()
{

}

void Policy::find_policy()
{
	if(current_policy_ == "sp1")
	{
		p1 p;
		p.run();
	}
	else if(current_policy_ == "sp2")
	{
		p2 p;
		p.run();
	}
	else if(current_policy_ == "sp3")
	{
		p3 p;
		p.run();
	}
}
