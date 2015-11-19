#pragma once
#include <opencv2/opencv.hpp>

class hiMat
{
	int width_;
	int height_;
	int stride_;
	unsigned int phy_addr_;
	void *vir_addr_;
	public:
		hiMat();
		// 开辟 mmz缓冲，初始化.
		hiMat(const cv::Mat &m);
		~hiMat();
		// 把数据赋于 m.
		void download(cv::Mat &m);
		// 为对象 开辟空间.
		void create(int width, int height, int type);
		hiMat& operator =(const hiMat& src);
		hiMat& operator =(const cv::Mat& m);
		
		int get_cols() const;
		int get_rows() const;
		int get_strie() const;
		unsigned int* get_phy_addr_p() const;
		void* get_vir_addr_p() const;
		int get_stride() const;
}
