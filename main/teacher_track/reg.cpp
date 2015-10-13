#include <map>
#include <string>
#include "reg.h"
#include "policy.h"

typedef std::map<std::string, Policy*> POLICIES;
POLICIES _all_policies;

void reg_policy(const char *name, Policy *policy)
{
	_all_policies[name] = policy;
}

Policy *find_policy(const char *name)
{
	POLICIES::const_iterator itf = _all_policies.find(name);
	if (itf == _all_policies.end()) {
		return 0;
	}
	else {
		return itf->second;
	}
}

