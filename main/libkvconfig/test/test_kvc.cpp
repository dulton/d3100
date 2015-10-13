#include <stdio.h>
#include <stdlib.h>
#include "../kvconfig.h"

int main()
{
	const char *fname = "libkvconfig/test/teacher_detect_trace.config";

	kvconfig_t *kvc = kvc_open(fname);
	if (!kvc) {
		fprintf(stderr, "ERR: can't open %s\n", kvc);
		return -1;
	}

	const char *key1 = "ptz_serial_name";
	const char *key2 = "test_cnt";
	const char *val;

	if (kvc_get(kvc, key1, 0, &val) < 0) {
		fprintf(stderr, "ERR: get err\n");
		return -1;
	}
	fprintf(stdout, "key='%s', val='%s'\n",
			key1, val);

	if (kvc_get(kvc, key2, "0", &val) < 0) {
		fprintf(stderr, "ERR: get err\n");
		return -1;
	}
	fprintf(stdout, "key='%s', val='%s'\n",
			key2, val);

	char buf[64];
	int v = atoi(val);
	v++;
	sprintf(buf, "%d", v);
	if (kvc_set(kvc, key2, buf) < 0) {
		fprintf(stderr, "ERR: set err\n");
		return -1;
	}

	if (kvc_save(kvc, 0) < 0) {
		fprintf(stderr, "ERR: save err\n");
		return 0;
	}

	kvc_close(kvc);

	fprintf(stdout, "OK\n");
	return 0;
}

