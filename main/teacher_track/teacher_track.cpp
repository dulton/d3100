/** 教师自动跟踪，依赖 libkvconfig, libptz, libteacher_detect */

#include <stdio.h>
#include "../libptz/ptz.h"
#include "../libkvconfig/kvconfig.h"
#include "../libteacher_detect/detect.h"
#include "udpsrv.h"
#include "cJSON.h"
#include "runtime.h"
#include "policy/p1.h"
#include "policy/p2.h"
#include "policy/p3.h"


int main(int argc, char **argv)
{
	// TODO: 根据配置文件，启动相应的策略.
	set_log_level(4);
	p3 p;
	p.run();

	return 0;
}

