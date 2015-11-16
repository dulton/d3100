/** ��ȡ�����ļ�����ʽΪ��
		# ע����
		key1=value1
		key 2=value 2

 */

#pragma once

#include <vector>
#include <string>
#include <map>
#include <cc++/thread.h>

#ifdef WIN32
#	ifdef LIBKVCONFIG_EXPORTS
#		define KVCAPI __declspec(dllexport)
#	else
#		define KVCAPI __declspec(dllimport)
#	endif
#else
#	define KVCAPI
#endif //

class KVCAPI KVConfig
{
	typedef std::map<std::string, std::string> KVS;
	KVS kvs_;
	ost::Mutex cs_;
	std::string filename_;

public:
	KVConfig(const char *filename);
	~KVConfig(void);

	const char *file_name() const { return filename_.c_str(); }

	/// �����Ƿ���� key
	bool has_key(const char *key);

	/// ���� key ��Ӧ�� value�����û���ҵ� key, �򷵻� def
	const char *get_value(const char *key, const char *def = 0);

	// ɾ�� key
	bool del_key(const char *key);

	/// ���� key �б�
	std::vector<std::string> keys();

	/// ֧���������޸� key=value����� value == 0����ɾ�� key����� key �Ѿ����ڣ��򸲸ǣ�
	int set_value(const char *key, const char *value);
	int set_value(const char *key, int v);

	/// ���浽ָ�����ļ���
	int save_as(const char *filename);
	int reload();

	void clear();	// ɾ������

private:
	void load_from_file(const char *filename);
};
