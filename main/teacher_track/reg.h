#pragma once

/** 注册策略 */

#include "policy.h"

// name 对应策略名字，如 sp1, sp4_bd ...之类 
void reg_policy(const char *name, Policy *policy);

#define REG_POLICY(name, p) \
	class __reg##name { \
	public: \
		__reg##name() { \
			reg_policy(#name, p); \
		} \
	}; \
	static __reg##name __reg_impl##name; 
