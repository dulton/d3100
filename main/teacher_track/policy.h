#pragma once

#include <string>
//#include "runtime.h"

#include "../libkvconfig/kvconfig.h"
#include "policy/p1.h"
#include "policy/p2.h"
#include "policy/p3.h"

/** 主程序根据配置.

  		Runtime *rt = ....;
  		Policy *policy = find_policy(cfg_policy_name);
		if (policy) {
			policy->run(rt);
		}
 */


class Policy
{

    kvconfig_t *kvc_;
	const char* current_policy_;
public:
	Policy(const char*filename = "global_trace.config");
	~Policy();

	void find_policy();

	//void run_policy();

};