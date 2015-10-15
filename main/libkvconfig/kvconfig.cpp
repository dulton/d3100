#include <map>
#include <string>
#include <stdio.h>
#include "kvconfig.h"

typedef std::map<std::string, std::string> KVS;

class KVConfig
{
	std::string fname_;	// 保存原始文件名字.
	std::string fname_session_;	// 对应的 .session 文件名字.

	KVS kvs_;

public:
	KVConfig(const char *fname)
	{
		fname_ = fname;
		fname_session_ = fname_ + ".session";

		load_from_file(fname_);
		load_from_file(fname_session_);
	}

	const char *get(const char *key, const char *def) const
	{
		KVS::const_iterator itf = kvs_.find(key);
		if (itf == kvs_.end())
			return def;
		else
			return itf->second.c_str();
	}

	void set(const char *key, const char *val)
	{
		kvs_[key] = val;
	}

	int save(const char *fname = 0) const
	{
		if (!fname) fname = fname_session_.c_str();

		FILE *fp = fopen(fname, "w");
		if (fp) {
			KVS::const_iterator it;
			for (it = kvs_.begin(); it != kvs_.end(); ++it) {
				fprintf(fp, "%s=%s\n", it->first.c_str(), it->second.c_str());
			}
			fclose(fp);
			return 0;
		}

		return -1;
	}

private:
	void load_from_file(const std::string &name)
	{
		// FIXME: 需要支持前后空格之类的 ...

		FILE *fp = fopen(name.c_str(), "r");
		if (fp) {
			while (!feof(fp)) {
				char buf[1024];
				char *p = fgets(buf, sizeof(buf), fp);
				if (!p) continue;
				if (*p == '#') continue;

				char key[64], val[512];
				if (sscanf(p, "%63[^=] = %511[^\r\n]", key, val) == 2) {
					kvs_[key] = val;
				}
			}

			fclose(fp);
		}
	}
};

kvconfig_t *kvc_open(const char *fname)
{
	return (kvconfig_t*)new KVConfig(fname);
}

void kvc_close(kvconfig_t *kvc)
{
	delete (KVConfig*)kvc;
}

const char *kvc_get(kvconfig_t *kvc, const char *key, const char *def)
{
	return ((KVConfig*)kvc)->get(key, def);
}

void kvc_set(kvconfig_t *kvc, const char *key, const char *val)
{
	((KVConfig*)kvc)->set(key, val);
}

int kvc_save(kvconfig_t *kvc, const char *fname)
{
	return ((KVConfig*)kvc)->save(fname);
}

