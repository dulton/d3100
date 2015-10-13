#pragma once

#include <string>
#include "runtime.h"

/** 主程序根据配置：

  		Runtime *rt = ....;
  		Policy *policy = find_policy(cfg_policy_name);
		if (policy) {
			policy->run(rt);
		}
 */

class Policy
{
public:
	virtual int run(Runtime *rt) = 0;
};

Policy *find_policy(const char *name);

