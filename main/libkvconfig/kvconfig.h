#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct kvconfig_t kvconfig_t;

kvconfig_t *kvc_open(const char *fname);
void kvc_close(kvconfig_t *kvc);

const char *kvc_get(kvconfig_t *kvc, const char *key, const char *def);
void kvc_set(kvconfig_t *kvc, const char *key, const char *val);

int kvc_save(kvconfig_t *kvc, const char *fname_as);

#ifdef __cplusplus
}
#endif // c++

