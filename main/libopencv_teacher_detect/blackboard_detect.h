#pragma once
#ifndef _blackboard_detect_h_
#define _blackboard_detect_h_

#include<opencv2/opencv.hpp>
#include<time.h>
#include "KVConfig.h"
using namespace cv;
using namespace std;


class BlackboardDetecting
{
	public:
	BlackboardDetecting(KVConfig *cfg);
	~BlackboardDetecting(void);
	double  diff_threshold_three;
	double  diff_threshold;
	int N;//��֡��ַ���buffer�еĸ���;
	int buflen;//˫֡��ַ���buffer�еĸ���;
	IplImage **buffer;
	KVConfig *cfg_;

	double merge_interval;
	double min_area;
	double max_area;

	BackgroundSubtractorMOG2 bg_model;
	double learning_rate;
	double mog_threshold;

	bool ismask_;
	Mat img_mask_;
	int video_width_;
	int video_height_;

	bool frame_difference_method (IplImage *image,std::vector<cv::Rect> &rect_vector);
	//IplImage *do_mask(const IplImage *img);
	void do_mask( Mat &img);
	cv::Rect masked_rect;


	std::vector < Rect > refineSegments2(Mat img, Mat & mask, Mat & dst,double interval,double minarea,double maxarea);
	void rect_fusion2(vector < Rect > &seq, double interval);
	Rect sort_rect(Rect a, Rect b);
	static int cmp_area(const Rect & a, const Rect & b);
	bool one_frame_bd( Mat img, vector < Rect > &r);


	void two_frame_method(IplImage*img,IplImage*silh);
	static int cmp_func_area(const CvRect&a,const CvRect&b);
	CvRect merge_rect(CvRect a,CvRect b);
	void rect_fusion(std::vector<cv::Rect> &seq,IplImage *image);
	void creat_buffer(IplImage *image);

	bool build_mask_internal(const char *key, Mat& img);
	Mat build_mask(const char *key,const char *key2 = 0);
	std::vector<cv::Point> load_roi(const char *pts);
	static int cmp_min_x(const Point & a, const Point & b);
	static int cmp_min_y(const Point & a, const Point & b);
	cv::Rect get_point_rect(std::vector<cv::Point> pt);
	cv::Rect get_rect(const char* pt1,const char* pt2);
};

#endif
