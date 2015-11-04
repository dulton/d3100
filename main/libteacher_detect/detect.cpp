#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "detect.h"
#include "../../hi353x/detect/mpi_wrap.h"
#include <pthread.h>
#include <string>	
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

struct detect_t
{
	hi31_t *hi31;

	TD td;
	RECT rects[4];
	
	bool is_quit;
	pthread_t thread_id;
	pthread_mutex_t mutex;
	std::string s;
};

static void erase_str(std::string &s)
{
	int len = s.size();
	if (s[len-3] == ',')
		s.erase(len-3, 1);
}
static std::string get_aims_str(TD *td, RECT *rects)
{
	std::stringstream ss;
	int i = 0;
	int j = 0;
	int x;
	int y;
	int width;
	int height;

	int length = sizeof(td->is_alarms) /4 ;
	printf("length = %d\n", length);
	ss<<"{\"stamp\":"<<td->stamp<<",\"rect\":[";
	for (int k = 0; k < 4; k++)
		printf("$$$$$$ %d\n", td->is_alarms[i]);
	for (i = 0; i < length; i++) {
		if ((i < length - 2)&&(td->is_alarms[i]==1)&&(td->is_alarms[i+1]==1)&&(td->is_alarms[i+2]==1)) {
			for (j=i; j<i+3; j++) {
				ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
			} 	
			i = i + 2;
		}
			
		else if ((i < length -1) && (td->is_alarms[i]==1)&&(td->is_alarms[i+1]==1)) {
			x = (rects[i].x + rects[i+1].x) / 2;
			y = (rects[i].y + rects[i+1].y) / 2;
			width = (rects[i].width + rects[i+1].width) / 2;
			height = (rects[i].height + rects[i+1].height) / 2;
			ss<< "{\"x\":"<<x<<",\"y\":"<<y \
					<< ",\"width\":"<<width<<",\"height\":" \
					<<height<<"},";
			i = i + 1;
		}
		else if(td->is_alarms[i] == 1) {
			ss<< "{\"x\":"<<rects[i].x<<",\"y\":"<<rects[i].y \
					<< ",\"width\":"<<rects[i].width<<",\"height\":" \
					<<rects[i].height<<"},";
		}
		else
			;
	}
	ss<<"]}";
	std::string s(ss.str());
	erase_str(s);
	return s;	
}

void *thread_proc(void *pdet)
{
	int ret;
	int i;
	detect_t *det = (detect_t*)pdet;	

	while (!det->is_quit) {
		TD td;
		ret = read_hi3531(det->hi31, &td);
		if (ret < 0) {
			usleep(10 * 1000);
			continue;
		}

#ifdef ZDEBUG
#endif // debug

		pthread_mutex_lock(&det->mutex);
		det->td = td;
#ifdef ZDEBUG
#endif // debug

		pthread_mutex_unlock(&det->mutex);

		usleep(200 * 1000);
	}

	return 0;
}

static void set_regions(RECT region, HI31_PS *ps)
{
	int i = 0;
	int x = region.x;
	int y = region.y;
	int width = region.width / 4;
	int height = region.height;


	for (i = 0; i < ps->vdas.num; i++) {
		ps->vdas.regions[i].rect.x = x + i * width;
		ps->vdas.regions[i].rect.y = y;
		ps->vdas.regions[i].rect.width = width;
		ps->vdas.regions[i].rect.height = height;
		ps->vdas.regions[i].st = 100;
		ps->vdas.regions[i].at = 45;
		ps->vdas.regions[i].ot = 1;
		ps->vdas.regions[i].ut = 0;
	}
}

detect_t *det_open(const char *kvfname)
{
	int ret;
	int x;
	int y;
	int width;
	int height;
	int i;
	RECT rect = {16, 17, 386, 32};
	HI31_PS ps;
	ps.vdas.size.width = 640;
	ps.vdas.size.height = 480;
	ps.vdas.num = 4;
	ps.chns.vi_chn = 16;
	ps.chns.vda_chn = 1;
	ps.vdas.image_file = "./background.yuv";	
	set_regions(rect, &ps);

	detect_t *det = new detect_t;
	det->is_quit = false;


	for (i=0; i++; i < ps.vdas.num) {
		det->rects[i] = ps.vdas.regions[i].rect;
	}

	ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
	}
	
	ret = open_hi3531(&det->hi31, ps);
	if (0 != ret) {
		PRT_ERR("err no 0x%x\n", ret);
	}
	pthread_create(&det->thread_id, 0, thread_proc, (void*)det);
	return det;
}

const char *det_detect(detect_t *det)
{
	detect_t detect;
	int len;
	
	pthread_mutex_lock(&det->mutex);
	det->s = get_aims_str(&det->td, det->rects);
	pthread_mutex_unlock(&det->mutex);

	return det->s.c_str();
}

void det_close(detect_t *det)
{
	det->is_quit = true;
	pthread_join(det->thread_id, 0);
	pthread_mutex_destroy(&det->mutex);
	close_hi3531(det->hi31);
	delete det;
}

#if 0
struct Range
{
	RECT r_;
	bool f_;

	Range(bool fired, RECT &r) : r_(r), f_(fired) {}
	Range(bool fired): f_(fired) {}

	bool is_fired() const { return f_; };	// 是否触发
	RECT pos() const { return r_; }		// 返回矩形位置
};

static void last_cnt(int cnt, int total, std::vector<int> &idx1, std::vector<int> &idx2)
{
	if (cnt == 2) {
		// 说明有两个区域激活，返回第一个的序号
		idx1.push_back(total - cnt);
	}
	else {
		// 0/1个，或者多个 ..
		for (int i = 0; i < cnt; i++) {
			idx2.push_back(total - cnt + i);
		}
	}
}

static std::vector<RECT> get_targets(const std::vector<Range> &ranges)
{
	/** 1. 如果相邻两个range都触发，则返回相邻两个的x的中点 ...
		2. 其他都返回触发的 ...

		可以先找出两个在一起的..
	 */

	std::vector<int> idx1, idx2;	// idx1 保存相邻的ranges中的第一个的序号，idx2 保存其它触发的 ..

	int cnt = 0;	// 连续次数
	for (size_t i = 0; i < ranges.size(); i++) {
		bool fired = ranges[i].is_fired();
		if (!fired) {
			// 不再连续
			last_cnt(cnt, i, idx1, idx2);
			cnt = 0;
		}
		else {
			cnt++;
		}
	}

	last_cnt(cnt, ranges.size(), idx1, idx2);

	fprintf(stderr, "merged idx: ");
	std::vector<RECT> rcs;
	for (size_t i = 0; i < idx1.size(); i++) {
		// 合并 ...x 可以使用中点，y 怎么办？返回第一个的吧
		RECT left = ranges[idx1[i]].pos();    // 左侧矩形
		RECT right = ranges[idx1[i]+1].pos(); // 右侧矩形
		fprintf(stderr, "%d, %d, ", idx1[i], idx1[i]+1);

		RECT rc(left.x + left.width/2, left.y, (left.width + right.width) / 2, left.height);
		rcs.push_back(rc);
	}

	fprintf(stderr, "\naloned idx: ");
	for (size_t i = 0; i < idx2.size(); i++) {
		// 独立的 ...
		fprintf(stderr, "%d, ", idx2[i]);
		RECT rc = ranges[idx2[i]].pos();	// 需要返回的矩形
		rcs.push_back(rc);
	}
	fprintf(stderr, "\n\n");

	return rcs;
}

static void test_1()
{
	// 没有触发
	std::vector<Range> no_fired;
	no_fired.push_back(Range(false));
	no_fired.push_back(Range(false));
	no_fired.push_back(Range(false));
	no_fired.push_back(Range(false));
	no_fired.push_back(Range(false));
	no_fired.push_back(Range(false));

	std::vector<RECT> rcs = get_targets(no_fired);

	// 没有俩连续的 ...
	std::vector<Range> no_merged;
	no_merged.push_back(Range(false));
	no_merged.push_back(Range(true));
	no_merged.push_back(Range(false));
	no_merged.push_back(Range(true));
	no_merged.push_back(Range(true));
	no_merged.push_back(Range(true));
	no_merged.push_back(Range(false));
	no_merged.push_back(Range(false));

	rcs = get_targets(no_merged);

	// 需要合并的
	std::vector<Range> merged;
	merged.push_back(Range(true));
	merged.push_back(Range(true));
	merged.push_back(Range(false));
	merged.push_back(Range(true));
	merged.push_back(Range(true));
	merged.push_back(Range(true));
	merged.push_back(Range(false));
	merged.push_back(Range(true));
	merged.push_back(Range(true));

	rcs = get_targets(merged);
}

#endif
