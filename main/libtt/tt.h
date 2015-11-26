#pragma once

typedef struct tt_t tt_t;

struct hiVIDEO_FRAME_INFO_S;

tt_t *tt_open(const char *cfgname);

/** who: 1 teacher, 2 student, 3 bd 
 */
void tt_vf(tt_t *t, int who, const hiVIDEO_FRAME_INFO_S *v);

void tt_close(tt_t *t);

