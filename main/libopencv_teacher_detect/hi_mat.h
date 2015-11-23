#pragma once
#include "config.h"
#include <opencv2/opencv.hpp>

class hiMat 
{
	int stride_;
	unsigned int phy_addr_;
	void *vir_addr_;

	size_t *ref_;
	bool outer_;

 public:

	enum Type
	{
		NONE = -1,
		RGB24,	// 8UC3
		SINGLE,	// 8U
		SP420,	// ??
	};

	hiMat();
	hiMat(const cv::Mat & m);
	hiMat(const hiMat & m);
	hiMat(unsigned int phy_addr, int width, int height, int stride, Type type);
	~hiMat();

	void download(cv::Mat & m);
	void create(int width, int height, Type type);

	hiMat & operator =(const hiMat & src);
	hiMat & operator =(const cv::Mat & m);

	void dump_hdr() const;
	void dump_data(const char *fname) const;

	int rows;
	int cols;
	Type type;

	unsigned int get_phy_addr() const;
	void *get_vir_addr() const;
	int get_stride() const;

	size_t ref()
	{
		if (ref_)
			return *ref_;
		else
			return -10000;
	}

	size_t memsize() const;

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
		           unsigned int max_value); // , IVE_THRESH_OUT_FMT_E type);

	void absdiff(const hiMat &src1, const hiMat &src2, hiMat &dst);

	void bit_or(const hiMat &src1, const hiMat &src2, hiMat &dst);

	void yuv2rgb(const hiMat &src, hiMat &dst);
}

