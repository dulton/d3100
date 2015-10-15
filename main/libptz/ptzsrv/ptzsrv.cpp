#include <cc++/socket.h>
#include <cc++/url.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include "../../teacher_track/cJSON.h"


#define IP "172.16.1.14"
#define SRV_PORT 10013


// -------------- 构造更方便些的 json 解析树.
struct util_TypedObject
{
	int type;	// 0: bv
	// 1: nv
	// 2: sv
	// 3: av
	// 4: ob
	// 5: null
	struct {
		bool bv;	// True, False
		double nv;	// number
		std::string sv;	// string
		std::vector<util_TypedObject> av;	// array
		std::map<std::string, util_TypedObject> ov; // object
	} data;
};

static void build_array(const cJSON *json, std::vector<util_TypedObject> &array);
static void build_object(const cJSON *json, std::map<std::string, util_TypedObject> &obj);

static std::string build_one(const cJSON *json, util_TypedObject &to)
{
	switch (json->type) {
		case 0:	// cJSON_False
		case 1:	// cJSON_True
			to.type = 0;
			to.data.bv = (json->type == cJSON_True);
			break;

		case 2: // cJSON_NULL
			to.type = 5;
			break;

		case 3: // cJSON_Number
			to.type = 1;
			to.data.nv = json->valuedouble;
			break;

		case 4: // cJSON_String
			to.type = 2;
			to.data.sv = json->valuestring;
			break;

		case 5: // cJSON_array
			to.type = 3;
			build_array(json->child, to.data.av);
			break;

		case 6: // cJSON_object
			to.type = 4;
			build_object(json->child, to.data.ov);
			break;
	}

	if (json->string)
		return json->string;
	else
		return "";	// 无名对象 ..
}

static util_TypedObject util_build_object_from_json_string(const char *json_str)
{
	util_TypedObject to = { 5 };	// null ..

	cJSON *json = cJSON_Parse(json_str);
	if (json) {
		build_one(json, to);
		cJSON_Delete(json);
	}

	return to;
}

static void build_array(const cJSON *json, std::vector<util_TypedObject> &array)
{
	while (json) {
		util_TypedObject to;
		build_one(json, to);
		array.push_back(to);
		json = json->next;
	}
}

static void build_object(const cJSON *json, std::map<std::string, util_TypedObject> &objs)
{
	while (json) {
		util_TypedObject to;
		std::string name = build_one(json, to);
		objs[name] = to;
		json = json->next;
	}
}

inline double util_now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

class TimeUsed
{
	double begin_, max_;
	std::string info_;

	public:
	TimeUsed(const char *info, double max_duration = 0.500)
	{
		if (info) info_ = info;
		else info_ = "NONE: ";

		max_ = max_duration;
		begin_ = util_now();
	}

	bool is_timeout() const
	{
		return util_now() - begin_ >= max_;
	}

	~TimeUsed()
	{
		double now = util_now();
		if (now - begin_ > max_) {
			fprintf(stderr, "<libptz>: WARNING: 云台操作超时：'%s', using %.3f (%.3f)\n", info_.c_str(), now-begin_, max_);
		}
	}
};

// ---------------- webservice + json 模式 ---------------------
static int wj_req(ost::URLStream &s, const char *url)
{
	TimeUsed tu(url);

	ost::URLStream::Error err = s.get(url);
	if (err)
		return err;

	std::stringstream ss;

	while (!s.eof()) {
		std::string x;
		s >> x;
		ss << x;
	}

	if (tu.is_timeout()) {
		fprintf(stderr, "\t:%s\n", ss.str().c_str());
	}

	// 根据 { "result":"ok",  } 进行判断.
	util_TypedObject to = util_build_object_from_json_string(ss.str().c_str());
	if (to.type == 4 && to.data.ov["result"].type == 2 && to.data.ov["result"].data.sv == "ok") {
		return 0;
	}
	else {
		fprintf(stderr, "<libptz> ERROR: 云台失败:'%s', str=%s\n", url, ss.str().c_str());
		return -1;
	}
}

static int wj_req_with_res(ost::URLStream &s, const char *url, std::ostream &os)
{
	TimeUsed tu(url);

	ost::URLStream::Error err = s.get(url);
	if (err)
		return err;

	std::stringstream ss;
	while (!s.eof()) {
		std::string x;
		s >> x;
		os << x;
		ss << x;
	}

	if (tu.is_timeout()) {
		fprintf(stderr, "\t:%s\n", ss.str().c_str());
	}

	// 根据 { "result":"ok",  } 进行判断.
	util_TypedObject to = util_build_object_from_json_string(ss.str().c_str());
	if (to.type == 4 && to.data.ov["result"].type == 2 && to.data.ov["result"].data.sv == "ok") {
		return 0;
	}
	else {
		fprintf(stderr, "<libptz> ERROR: 云台失败:'%s', str=%s\n", url, ss.str().c_str());
		return -1;
	}
}

// -------------- 构造更方便些的 json 解析树.
struct TypedObject
{
	int type;	// 0: bv
	// 1: nv
	// 2: sv
	// 3: av
	// 4: ob
	// 5: null
	struct {
		bool bv;	// True, False
		double nv;	// number
		std::string sv;	// string
		std::vector<TypedObject> av;	// array
		std::map<std::string, TypedObject> ov; // object
	} data;
};

typedef std::map<std::string, TypedObject> JSONDATA;

static void build_array(const cJSON *json, std::vector<TypedObject> &array);
static void build_object(const cJSON *json, std::map<std::string, TypedObject> &obj);

static std::string build_one(const cJSON *json, TypedObject &to)
{
	switch (json->type) {
		case 0:	// cJSON_False
		case 1:	// cJSON_True
			to.type = 0;
			to.data.bv = (json->type == cJSON_True);
			break;

		case 2: // cJSON_NULL
			to.type = 5;
			break;

		case 3: // cJSON_Number
			to.type = 1;
			to.data.nv = json->valuedouble;
			break;

		case 4: // cJSON_String
			to.type = 2;
			to.data.sv = json->valuestring;
			break;

		case 5: // cJSON_array
			to.type = 3;
			build_array(json->child, to.data.av);
			break;

		case 6: // cJSON_object
			to.type = 4;
			build_object(json->child, to.data.ov);
			break;
	}

	if (json->string)
		return json->string;
	else
		return "";	// 无名对象 ..
}

static void build_array(const cJSON *json, std::vector<TypedObject> &array)
{
	while (json) {
		TypedObject to;
		build_one(json, to);
		array.push_back(to);
		json = json->next;
	}
}

static void build_object(const cJSON *json, std::map<std::string, TypedObject> &objs)
{
	while (json) {
		TypedObject to;
		std::string name = build_one(json, to);
		objs[name] = to;
		json = json->next;
	}
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

static void srv(int fd)
{
	static ost::URLStream s;

	// 接收命令，转发到 http srv
	char buf[1024];	// FIXME: ...
	int rc = recv(fd, buf, sizeof(buf), 0);
	if (rc <= 0) {
		fprintf(stderr, "ERR: recv err\n");
		return;
	}

	buf[rc] = 0;
	std::vector<std::string> ss;
	split(buf, '&', ss);

	fprintf(stderr, "recv: '%s'\n", buf);

	if (ss.size() < 2) {
		fprintf(stderr, "cmd ERR!!!: '%s'\n", buf);
		return;
	}

	strcpy(buf, "result ...");

	const char *who = ss[1].c_str()+4; // Who=xxx
	char url[256];

	if (ss[0] == "PtzCmd=Turn") {
		int speed = 1;
		sscanf(ss[3].c_str(), "Speed=%d", &speed);

		if (ss[2] == "Direction=left") {
			snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/left?speed=%d", IP, who, speed);
			wj_req(s, url);
		}
		else if (ss[2] == "Direction=right") {
			snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/right?speed=%d", IP, who, speed);
			wj_req(s, url);
		}
		else if (ss[2] == "Direction=up") {
			snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/up?speed=%d", IP, who, speed);
			wj_req(s, url);
		}
		else if (ss[2] == "Direction=down") {
			snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/down?speed=%d", IP, who, speed);
			wj_req(s, url);
		}
	}
	else if (ss[0] == "PtzCmd=StopTurn") {
		snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/stop", IP, who);
		wj_req(s, url);
	}
	else if (ss[0] == "PtzCmd=TurnToPos") {
		int x, y, sx, sy;
		sscanf(ss[2].c_str(), "X=%d", &x);
		sscanf(ss[3].c_str(), "Y=%d", &y);
		sscanf(ss[4].c_str(), "SX=%d", &sx);
		sscanf(ss[5].c_str(), "SY=%d", &sy);

		snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/set_pos?x=%d&y=%d&sx=%d&sy=%d",
				IP, who, x, y, sx, sy);
		wj_req(s, url);
	}
	else if (ss[0] == "PtzCmd=SetZoom") {
		int z;
		sscanf(ss[2].c_str(), "Zoom=%d", &z);
		
		snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/set_zoom?z=%d", z);
		wj_req(s, url);
	}
	else if (ss[0] == "PtzCmd=GetPos") {
		std::stringstream os;
		snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/get_pos", IP, who);
		wj_req_with_res(s, url, os);

		cJSON *json = cJSON_Parse(os.str().c_str());

		TypedObject to;
		build_one(json, to);
		cJSON_Delete(json);

		if (to.data.ov["result"].data.sv == "ok") {
			int h = (int)(to.data.ov["value"].data.ov["data"].data.ov["x"].data.nv);
			int v = (int)(to.data.ov["value"].data.ov["data"].data.ov["y"].data.nv);
			sprintf(buf, "X=%d&Y=%d", h, v);
		}
		else {

		}
	}
	else if (ss[0] == "PtzCmd=GetZoom") {
		std::stringstream os;
		snprintf(url, sizeof(url), "http://%s:10003/ptz/%s/get_zoom", IP, who);
		wj_req_with_res(s, url, os);

		cJSON *json = cJSON_Parse(os.str().c_str());

		TypedObject to;
		build_one(json, to);
		cJSON_Delete(json);

		if (to.data.ov["result"].data.sv == "ok") {
			int z = (int)(to.data.ov["value"].data.ov["data"].data.ov["z"].data.nv);
			sprintf(buf, "Zoom=%d", z);
		}
		else {

		}
	}
	else {
		fprintf(stderr, "unknown cmd: '%s'\n", ss[0].c_str());
	}

	send(fd, buf, strlen(buf), 0);
}


int main()
{
	/** 启动一个 tcpserver，模拟3100版本的云台服务，将命令转发到 python 版本的云台服务
	 */

	int fd0 = socket(AF_INET, SOCK_STREAM, 0);
	if (fd0 < 0) {
		fprintf(stderr, "ERR: create sock err\n");
		return -1;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(SRV_PORT);
	sin.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(fd0, (sockaddr*)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "ERR: bind err\n");
		return -1;
	}

	listen(fd0, 5);

	while (1) {
		sockaddr_in from;
		socklen_t len = sizeof(from);

		int fd = accept(fd0, (sockaddr*)&from, &len);
		if (fd < 0) {
			fprintf(stderr, "ERR: accept err\n");
			continue;
		}

		srv(fd);

		close(fd);
	}

	return 0;
}

