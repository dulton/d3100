#pragma once
#ifndef _detect_t_h_
#define _detect_t_h_

#include<opencv2/opencv.hpp>
#include<time.h>
#include "KVConfig.h"

using namespace cv;
using namespace std;

#include  "hi_comm_ive.h"
#include  "hi_type.h"
#include  "mpi_ive.h"
#include "mpi_sys.h"
#include "hi_common.h"
#include"hi_comm_sys.h"
#include "hi_comm_video.h"
#include "mpi_vb.h"
#include "mpi_vi.h"

//��ʱ�ṹ��;
struct Static_s
{
	bool flag;
	Rect r;
	double pre_time;
	double cur_time;
	double continued_time;
	Static_s();

	//Static_s()
	//{
	//	flag = false;
	//}
};

//��䱳���㷨�Ľṹ��;
struct Fill_Bg
{
	//bool ud_static_begain;//�Ƿ�ո����꾲ֹĿ��;
	bool body_move;

	double continued_time;//�ж��غϵ�ʱ�䣨10��;
	double continued_time_long;//�жϾ�ֹ�ĵڶ��ַ����ĸ���ʱ��;
	//double norect_update_time;//��Ŀ��೤ʱ��͸�����������;
	
	bool isfillok;//����������µı�־;
	bool isfillok_end;//���������Ƿ������ȷ�ı�־;
	int filltwice_num;//������������Ŀ�����;

	int nframe;//�ᴩ����;
	int num;//��ȡ��10֡��Ϊ��ʼ����ͼ;

	double mog2_interval1;//��Ŀ���ںϼ�ࣨ��ȫ�������֮��;
	double mog2_interval2;//���һ�˵�С���ںϼ�ࣨ�ڸ������֮ǰ��;
	double second_interval;//�������������ʱ���и���;

	double body_width;//����ͼ���д�ſ��(����Ϊ��λ);
	//double w_max;//bg2�е�һ��������Ҫ��չ�ľ���;
	//double h_max;

	//std::vector<Rect> fill_nowrect;//��ǰ�ո��µľ��ο򣨣���������;
	std::vector<Rect> fist_fillrect;//��չ������;
	//std::vector<Rect> fist_fillrect_noclear;
	//std::vector<Rect> fist_fillrect_original;//��һ����չ��ľ���(δ������ͼ��������);
	//std::vector<Rect> fist_fillrect_t;//��һ����չ��ľ���;
	std::vector<Rect> rect_old;//����������õľ���;
	Mat bg;//����ͼ;	
	//Static_s no_rect;
	Static_s mog2_s;//mog2�㷨��ʱ���ж�;

};

//struct CT_Face
//{
//	CvMemStorage* storage; 
//    CvHaarClassifierCascade* cascade;
//	const char* cascade_name;
//	std::vector<Rect> face_rect;
//	std::vector<Rect> ct_rect;
//	bool is_face_ok;//�Ƿ��⵽����;
//	bool is_ct_ok;//�Ƿ��ʼ��ct��;
//};


//���������ṹ;
struct Region
{
	double cur_tbg;
	double pre_tbg;
	double continuetime_bg;
	bool flage_bg;//��ʼ��ʱ�ı�־;
	bool has_frame_rect;//�Ƿ���֡���;
	bool has_old_rect;
	Rect region;//���ڵķ���;
	int num;	
	double cur_static;
	double pre_static;
	double continuetime_static;
	bool flag_static;//��ʼ��ʱ�ı�־;
};

//����֡����±���ͼ;
struct Update_Bg
{
    std::vector<Region> region;
	double time;//��Ŀ�������Ŀ��ʱ��;
	double multiple_target_time;//��Ŀ�����ʱ��;
	int region_num;//������Ŀ;//������������1,2,3....
	double region_interval;//�������;
	double slow_learn_rate;//��Ŀ�����򱳾���������;
	double fast_learn_rate;//��Ŀ��������ٸ���;
};

//֡�;
struct Frame_struct
{
	int N;
	Mat buffer[5];	
	int threshold_three;//��֡��ַ���ֵ;
	int threshold_two;//˫֡��ַ���ֵ;
	std::vector<Rect> frame_rect;
	std::vector<Rect> masked_frame_rect;
	int interval;
	int minarea;
	int minrect;
	bool is_body_down;
	int bottom_inter;//����궨���ο��·����ؾ���;
};

struct Upbody_Update
{
	Mat upbody_bg;
	Update_Bg bg_upbody;//&&&&&&&&&&;
	int Y_value;
	std::vector<cv::Rect> upbody_rect;
	int min_area;
	int min_rect_area;
	bool is_upbody;
	int upbody_u_max;
	int upbody_v_max;
	float region_interval;
	int frame_num;
};


class TeacherDetecting
{
	BackgroundSubtractorMOG2 bg_model;//�ɸ���һЩ�ڲ�����;
	float mog_learn_rate;

	int video_width_;
	int video_height_;
	KVConfig *cfg_;

	//CompressiveTracker ct;
	//CT_Face ct_face_s;
	Frame_struct frame_s;
	Update_Bg ud_bg_s;
	Fill_Bg  fillbg_struct;
	

	double min_area;
	double min_rect_area;
	double luv_u_max;
	double luv_v_max;
	double luv_L;

	bool ismask_;
	Mat img_mask_;//ԭ����ԭʼ��С����ͼ�񣬺�ĳɱ궨������ͼ��;

public:
	Upbody_Update up_update;
	TeacherDetecting(KVConfig *cfg);
	~TeacherDetecting(void);
	bool one_frame_luv(Mat raw_img,Mat img, vector<Rect> &r,vector<Rect> &first_rect);
	void upbody_bgupdate( Mat upbody_img);
    void get_upbody(Mat img_upbody );
	void upbody_luv_method(const Mat &img );
	void upbody_updatebg_slow(Mat img,Rect r,double learn_rate);
	void mog_method(Mat img);
	void do_mask(Mat &img);
	cv::Rect masked_rect;
	cv::Rect upbody_masked_rect;//&&&&&&&&&&&&;

	//hi3531
	HI_S32 SAMPLE_IVE_INIT();
	HI_S32 hi_luv_method( std::vector<Mat> img,std::vector<Mat> bg,Mat &dst);
	HI_S32 hi_dilate( Mat src, Mat &dst);
	HI_S32 hi_blur( Mat src, Mat &dst);
	HI_S32 hi_two_frame_method( Mat src, Mat &dst);

protected:
	//������ct;
	/*std::vector<Rect> face_detect_and_draw( IplImage* image);
	std::vector<Rect> face_detecting(Mat image,Rect rect);
	void init_ct(Mat raw_image,Rect rect);
	void is_right_ct( Mat raw_img,Mat raw_gray,std::vector<Rect> rect_old);
	void facedetect_ctinit(Mat raw_image, Rect rect_bg);*/
	void init_fillbg_struct(Fill_Bg &fillbg_struct);

	//֡�;
	void creat_buffer(IplImage *image);
	//void two_frame_method(Mat img,Mat &silh);
	void two_frame_method(Mat img,Mat &silh,Mat Y);
	void three_frame_method(Mat img,Mat &silh);
	//void frame_difference_method (Mat raw_image,std::vector<Rect> &full_rect,std::vector<Rect> &masked_rect);
	void frame_difference_method (Mat image,std::vector<Rect> &masked_frame_rect,Mat Y);
	void init_frame_struct(Frame_struct &frame_s);

	//��̬���±���;
	void frame_updatebg(Mat image);
	void reset(Update_Bg &bg);
	void reset_upbody(Update_Bg &bg);//&&&&&&&&&&&&;
	void updatebg_slow(Mat img,Rect r,double learn_rate);
    void reset_static_region( Region &region );
	void reset_region( Region &region );
	void frame_updatebg(Mat raw_img,Mat image);

	//����������;
	void luv_method(const Mat &img ,std::vector<Mat> img_t_vec);
	void fillbg_LUV( Mat img);
	void is_teacher_down(Mat raw_img,Mat img2);
	void updatebg(Mat img,Rect r);
	void is_need_fillbg_twice(Mat img);
	void update_bg_again(Mat img);
	void norect_update_bg( Mat img);
	void eliminate_longrect(Mat img );

	//�ں�;
	static int cmp_area(const Rect&a,const Rect&b);
	Rect sort_rect(Rect a,Rect b);
	vector<Rect> refineSegments2(Mat img, Mat& mask,Mat &dst,double interval,double min_area,double rect_area);
	void rect_fusion2(vector<Rect> &seq,double interval);
	std::vector < Rect > upbody_refineSegments2(Mat img, Mat & mask,
						       Mat & dst,
						       double interval,double marea,double mrect_area);

	//����;
	bool build_mask_internal(const char *key, Mat& img);
	Mat build_mask(const char *key,const char *key2 = 0);
	std::vector<cv::Point> load_roi(const char *pts);
	static int cmp_min_x(const Point & a, const Point & b);
	static int cmp_min_y(const Point & a, const Point & b);
	cv::Rect get_point_rect(std::vector<cv::Point> pt);
	cv::Rect get_rect(const char* pt1,const char* pt2);
	void init_mask( );
};

#endif
