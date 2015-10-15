#include <stdio.h>
#include <stdlib.h>
#include "../fsm.h"

// 状态
enum State
{
	ST_Init,
	ST_1,
	ST_2,
	ST_3,
	ST_End,
};

// 事件
enum Event
{
	EV_Next,
	EV_Prev,
};

// 转换函数
static int do_next_0(void *c);
static int do_next_1(void *c);
static int do_next_2(void *c);
static int do_next_3(void *c);
static int do_prev_1(void *c);

// 状态转换表
static fsm_transiton _trans[] = {
	{ ST_Init, EV_Next, do_next_0 },
	{ ST_1, EV_Next, do_next_1 },
	{ ST_1, EV_Prev, do_prev_1 },
	{ ST_2, EV_Next, do_next_2 },
	{ ST_3, EV_Next, do_next_3 },
};
static int _cnt = sizeof(_trans) / sizeof(fsm_transiton);

// 下一个事件
static int next_event(void *c);

// 全局变量 ...
class Context
{

};

int main()
{
	Context ctx;
	fsm_table table = { _trans, _cnt, ST_Init, ST_End, next_event };

	fsm_t *fsm = fsm_open(&ctx, &table);
	if (fsm) {
		fsm_run(fsm);
		fsm_close(fsm);
	}

	return 0;
}

static int next_event(void *c)
{
	return EV_Next;
}

static int do_next_0(void *c)
{
	Context *ctx = (Context*)c;
	// ... 
	fprintf(stderr, "%s\n", __func__);
	return ST_1;
}

static int do_next_1(void *c)
{
	fprintf(stderr, "%s\n", __func__);
	return ST_2;
}

static int do_next_2(void *c)
{
	fprintf(stderr, "%s\n", __func__);
	return ST_3;
}

static int do_next_3(void *c)
{
	fprintf(stderr, "%s\n", __func__);
	return ST_End;
}

static int do_prev_1(void *c)
{
	fprintf(stderr, "%s\n", __func__);
	return ST_Init;
}

