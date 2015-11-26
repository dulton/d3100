#pragma once

#include "config.h"
#ifdef WITHOUT_OCV
namespace cv
{
	struct Rect
	{
		int x, y, width, height;
	};
};
#else
#include <opencv2/opencv.hpp>
#endif // with out opencv

class hiMat 
{
	int stride_, hi_stride_;
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
		U64,	// 用于积分图 ..., 第28位为 sum, 高 36位为"平方和" ..
	};

	hiMat();
#ifndef WITHOUT_OCV
	hiMat(const cv::Mat & m);
	hiMat(const cv::Mat &m, const cv::Rect &roi);
#endif // without opencv
	hiMat(const hiMat & m);
	hiMat(const hiMat &m, const cv::Rect &roi);
	hiMat(unsigned int phy_addr, int width, int height, int stride, Type type);
	~hiMat();

#ifndef WITHOUT_OCV
	void download(cv::Mat & m);
#endif // without opencv
	void create(int width, int height, Type type);

	template<typename T> T *ptr(int row)
	{
		return (T*)((char*)vir_addr_ + row * stride_);
	}
	template<typename T> const T *ptr(int row) const
	{
		return (const T*)((char*)vir_addr_ + row * stride_);
	}
	template<typename T> T &at(int row, int col)
	{
		return ptr<T>(row)[col];
	}
	template<typename T> const T &at(int row, int col) const
	{
		return ptr<T>(row)[col];
	}

	hiMat & operator =(const hiMat & src);
	hiMat & operator =(const cv::Mat & m);

	hiMat clone() const; 

	hiMat operator()(const cv::Rect &roi) const;

	void dump_hdr() const;
	void dump_data(const char *fname) const;
	
	void flush() const;

	int rows;
	int cols;
	Type type;

	unsigned int get_phy_addr() const;
	void *get_vir_addr() const;
	int hi_stride() const { return hi_stride_; }// 返回 hi3531 函数使用的 stride 概念 ...
	int stride() const { return stride_; } // 返回一行占用字节 ...

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
	void deepcp(hiMat &m) const; // 
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
	void integral(const hiMat &src, hiMat &dst);
}

