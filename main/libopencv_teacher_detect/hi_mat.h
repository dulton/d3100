#pragma once
#include "config.h"
#include <opencv2/opencv.hpp>

class hiMat 
{
	int stride_;
	unsigned int phy_addr_;
	void *vir_addr_;

	size_t *ref_;		// 引用计数 ..

 public:
	hiMat();
	// 开辟 mmz缓冲，初始化.
	hiMat(const cv::Mat & m);
	hiMat(const hiMat & m);
	~hiMat();

	// 把数据赋于 m.
	void download(cv::Mat & m);

	// 为对象 开辟空间.
	void create(int width, int height, size_t bytes_of_line);

	hiMat & operator =(const hiMat & src);
	hiMat & operator =(const cv::Mat & m);

	int rows;
	int cols;

	unsigned int get_phy_addr() const;
	void *get_vir_addr() const;
	int get_stride() const;

#ifdef DEBUG_HIMAT
	size_t ref()
	{
		if (ref_)
			return *ref_;
		else
			return -10000;
	}
#endif

 private:
	void release();
	void addref();
};

namespace hi
{
	void dilate(const hiMat &src, hiMat &dst);

	void erode(const hiMat &src, hiMat &dst);

	void filter(const hiMat &src, hiMat &dst);

	void threshold(const hiMat &src, hiMat &dst, unsigned int threshold, 
		           unsigned int max_value, IVE_THRESH_OUT_FMT_E type);

	void absdiff(const hiMat &src1, const hiMat &src2, hiMat &dst);

	void bit_or(const hiMat &src1, const hiMat &src2, hiMat &dst);

	void yuv2rgb(const hiMat &src, hiMat &dst);
}

