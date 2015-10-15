#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include "fsm.h"
#include "cJSON.h"

static double util_now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

FSMEvent::FSMEvent(int id)
	: id_(id)
{
	stamp_ = util_now();
}

static std::vector<DetectionEvent::Rect> _parse_rects(cJSON *j)
{
	std::vector<DetectionEvent::Rect> rcs;

	cJSON *c = j->child;

	while (c) {
		cJSON *r = c->child;

		DetectionEvent::Rect rc;

		while (r) {
			if (!strcmp("x", r->string))
				rc.x = r->valueint;
			if (!strcmp("y", r->string))
				rc.y = r->valueint;
			if (!strcmp("width", r->string))
				rc.width = r->valueint;
			if (!strcmp("height", r->string))
				rc.height = r->valueint;

			r = r->next;
		}

		rcs.push_back(rc);

		c = c->next;
	}

	return rcs;
}

void DetectionEvent::parse_json(const char *str)
{
	if (who_ == "teacher") {
		/*
			{"stamp":12345,"rect":[{"x":0,"y":0,"width":100,"height":100},
	                               {"x":0,"y":0,"width":100,"height":100}]}
		 */
		cJSON *j = cJSON_Parse(str);
		if (j) {
			cJSON *c = j->child;
			while (c) {
				if (!strcmp("stamp", c->string)) {
					stamp_ = c->valuedouble;
				}
				else if (!strcmp("rect", c->string)) {
					targets_ = _parse_rects(c);
				}

				c = c->next;
			}
			cJSON_Delete(j);
		}		
	}
}

