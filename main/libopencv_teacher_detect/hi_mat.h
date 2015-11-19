#pragma once
#include <opencv2/opencv.hpp>

class hiMat 
{
	int stride_;
	unsigned int phy_addr_;
	void *vir_addr_;

	bool using_ref_;	// 是否使用引用计数 ..
	size_t ref_;		// 引用计数 ..

 public:
	hiMat();
	// 开辟 mmz缓冲，初始化.
	hiMat(const cv::Mat & m);
	~hiMat();

	// 把数据赋于 m.
	void download(cv::Mat & m);

	// 为对象 开辟空间.
	void create(int width, int height, int type);

	hiMat & operator =(const hiMat & src);
	hiMat & operator =(const cv::Mat & m);

	int rows;
	int cols;

	unsigned int get_phy_addr() const;
	void *get_vir_addr() const;
	int get_stride() const;
};

