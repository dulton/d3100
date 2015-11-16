#include"blackboard_detect.h"


BlackboardDetecting::BlackboardDetecting(KVConfig *cfg)	
	: cfg_(cfg)
{
	
	N = 4;
	buflen = atof(cfg_->get_value("buflen", "3"));
	buffer = NULL;
	diff_threshold_three = atof(cfg_->get_value("diff_threshold_three", "17"));
	diff_threshold = atof(cfg_->get_value("diff_threshold", "30"));
	merge_interval = atof(cfg_->get_value("merge_interval", "30"));
	min_area = atof(cfg_->get_value("min_area", "10"));
	max_area = atof(cfg_->get_value("max_area", "10000"));
	video_width_ = atof(cfg_->get_value("video_width", "960"));
	video_height_ = atof(cfg_->get_value("video_height", "540"));

	learning_rate = atof(cfg_->get_value("learning_rate", "0.0005"));
	mog_threshold = atof(cfg_->get_value("mog_threshold", "150"));

	bg_model.set("fTau",atof(cfg_->get_value("fTau", "0.2")));//\D2\F5Ӱ\CF\FB\B3\FD\B2\CE\CA\FD\A3\AC0-1֮\BC䣬Ĭ\C8\CFΪ0.5\A3\ACԽС\A3\AC\D2\F5Ӱ\CF\FB\B3\FDԽ\C0\F7\BA\A6
	bg_model.set("varThreshold",atof(cfg_->get_value("varThreshold", "40")));

	ismask_ = false;

	masked_rect = get_rect(cfg_->get_value("calibration_data", "0"),cfg_->get_value("calibration_data_2", "0"));

	const char*cb_date,*cb_date_2;
	if(cfg_->get_value("calibration_data", "0"))
		cb_date = "calibration_data";
	else
		cb_date = NULL;
	if(cfg_->get_value("calibration_data_2", "0"))
		cb_date_2 = "calibration_data_2";
	else
		cb_date_2 = NULL;
	img_mask_ = build_mask(cb_date,cb_date_2);

}

BlackboardDetecting::~BlackboardDetecting()
{
}

void BlackboardDetecting::creat_buffer(IplImage *image)
{
	buffer = (IplImage**)malloc(sizeof(buffer[0])*N);
	//buffer = new IplImage*[N];
	for(int i = 0;i<N;i++)
	{
		buffer[i]= cvCreateImage(cvSize(image->width,image->height),IPL_DEPTH_8U,1);
		cvSetZero(buffer[i]);
	}
}
//**************************\C1\BD֡\B2\EE\B7ַ\A8***********************************
//\CA\E4\C8룺img//\B5\B1ǰͼ\CF\F1;
//\CA\E4\B3\F6\A3\BAsilh//\BBҶ\C8ͼ\A3\A8\B5\B1ǰ֡ͼ\CF\F1\BAͱ\A3\B4\E6\B5\C4\C9\CFһ֡ͼ\CF\F1\CF\E0\BC\F5\BA\F3ͼ\CF\F1\A3\A9
//buflen\C9\E8Ϊ2\A3\BA\B1\EDʾ\C1\AC\D0\F8\C1\BD֮֡\BC\E4\D7\F6֡\B2\C9\E8Ϊ3\A3\BA\B1\EDʾ\B8\F4һ֡\D7\F6֡\B2\EE;
void BlackboardDetecting:: two_frame_method(IplImage*img,IplImage*silh)
{
	cvCvtColor(img,buffer[buflen-1],CV_BGR2GRAY);
	cvAbsDiff(buffer[buflen-1],buffer[0],silh);
	for(int i = 0;i<buflen-1;i++)
	{
		cvCopy(buffer[i+1],buffer[i]);
	}
	cvThreshold( silh, silh, diff_threshold, 255, CV_THRESH_BINARY );
	cvSmooth(silh,silh,CV_MEDIAN,3,0,0,0);

	/*IplImage* pyr=cvCreateImage(cvSize((silh->width&-2)/2,(silh->height&-2)/2),IPL_DEPTH_8U,1);
	cvPyrDown(silh,pyr,7);
	cvDilate(pyr,pyr,0,1);
	cvPyrUp(pyr,silh,7);
	cvReleaseImage(&pyr);*/

	cv::dilate((Mat)silh, (Mat)silh, cv::Mat());
	cv::erode((Mat)silh,(Mat)silh, cv::Mat());   
	cv::erode((Mat)silh, (Mat)silh, cv::Mat()); 
	cv::dilate((Mat)silh, (Mat)silh, cv::Mat(),cv::Point(-1,-1),2); 
}

//******************\BE\D8\D0ΰ\B4\C3\E6\BB\FD\D3ɴ\F3\B5\BDС\C5\C5\D0\F2\B5ģ\A8\B1ȽϺ\AF\CA\FD\A3\A9******************
//\CA\E4\C8룺Rect a \BA\CD Rect b;
//\CA\E4\B3\F6\A3\BA\B5\B1a\B5\C4\C3\E6\BB\FDС\D3\DAb\B5\C4\C3\E6\BB\FDʱ\CA\E4\B3\F61;
int BlackboardDetecting::cmp_func_area(const CvRect&a,const CvRect&b) 
{ 	

	return (a.width*a.height)>(b.width*b.height);

}

//**************************\C1\BD\BE\D8\D0ο\F2\BD\F8\D0\D0\C8ں\CF******************************
//\CA\E4\C8룺\C1\BD\B8\F6\BE\D8\D0\CEa\A1\A2b;
//\B7\B5\BBأ\BA\C1\BD\B8\F6\BE\D8\D0\CE\C8ں\CF֮\BA\F3\B5ľ\D8\D0ο\F2;
CvRect BlackboardDetecting::merge_rect(CvRect a,CvRect b)
{
	CvRect rect_new;
	int small_x = a.x;
	int small_y = a.y;
	int big_x = a.x+a.width;
	int big_y = a.y+a.height;
	if(b.x<small_x)
		small_x=b.x;
	if(b.y<small_y)
		small_y=b.y;
	if(b.x+b.width>big_x)
		big_x=b.x+b.width;
	if(b.y+b.height>big_y)
		big_y=b.y+b.height;
	rect_new = cvRect(small_x,small_y,big_x-small_x,big_y-small_y);
	return rect_new;
}

//*****************************\D4˶\AF\C7\F8\D3\F2\C8ں\CF*******************************
//\CA\E4\C8룺seq:\D4˶\AF\BE\D8\D0\CE\D0\F2\C1\D0 \A3\ACimage:ͼ\CF\F1;
//\CA\E4\B3\F6\A3\BAseq:\C8ں\CF֮\BA\F3\B5\C4\D4˶\AF\BE\D8\D0\CE\D0\F2\C1\D0;
void BlackboardDetecting::rect_fusion(std::vector<cv::Rect> &seq,IplImage *image)
{
	//\B6\D4\D4˶\AF\BF\F2\CFȰ\B4\C3\E6\BB\FD\B4\F3С\BD\F8\D0\D0\C5\C5\D0\F2\A3\AC\D3ɴ\F3\B5\BDС;
	std::sort(seq.begin(),seq.end(),cmp_func_area);
	CvRect rect_i;
	CvRect rect_j;
	int num = 0;int interval = merge_interval;
	for(;;)
	{
		std::vector<cv::Rect>::iterator it1;
		std::vector<cv::Rect>::iterator it2;
		num=0;
		for(it1 = seq.begin();it1!= seq.end();)
		{
			for(it2 = it1+1;it2!= seq.end();)
			{
				rect_i = *it1;
				rect_j = *it2;
				if(rect_i.x>(rect_j.x+rect_j.width+interval)||(rect_i.x+rect_i.width+interval)<rect_j.x
					||rect_i.y>(rect_j.y+rect_j.height+interval)||(rect_i.y+rect_i.height+interval)<rect_j.y)
				{
					it2++;
					continue;
				}
				else//\B5\B1\BE\D8\D0ο\F2֮\BC\E4\D3н\BB\BC\AFʱ\BD\F8\D0\D0\C8ں\CF;
				{
					*it1 = merge_rect(rect_i,rect_j);
					it2 = seq.erase(it2);
					num++;
				}
			}
			it1++;
		}
		//\B5\B1\CB\F9\D3о\D8\D0ο\F2֮\BC䲻\BF\C9\D4\D9\C8ں\CFʱֹͣѭ\BB\B7;
		if(num==0)
		{
			//\D6\D8\D0°\B4\C3\E6\BB\FD\C5\C5\D0\F2;
			std::sort(seq.begin(),seq.end(),cmp_func_area);
			break;
		}		

	}

}


bool BlackboardDetecting:: frame_difference_method (IplImage *image,std::vector<cv::Rect> &rect_vector)
{
	bool has_rect = false;
	CvMemStorage *siln_contours_storage = NULL;
	CvSeq *siln_contours_seq=NULL;
	if(!siln_contours_storage)
	{
		siln_contours_storage=cvCreateMemStorage(0);
		siln_contours_seq=cvCreateSeq(0,sizeof(CvSeq),sizeof(CvContour),siln_contours_storage);
	}
	else
		cvClearMemStorage(siln_contours_storage);
	CvSize size = cvSize(image->width,image->height);
	IplImage* silh=NULL;
	silh=cvCreateImage(size,IPL_DEPTH_8U,1);
	if(!buffer)
	{
		creat_buffer(image);
	}
	two_frame_method(image,silh);	
	cvFindContours(silh,siln_contours_storage,&siln_contours_seq,sizeof(CvContour),
		CV_RETR_EXTERNAL,CV_CHAIN_APPROX_SIMPLE,cvPoint(0,0));
	for(;siln_contours_seq;siln_contours_seq=siln_contours_seq->h_next)
	{
		CvRect rect = ((CvContour*)siln_contours_seq)->rect;
		if(rect.x<0||rect.y<0||rect.x+rect.width>image->width||rect.y+rect.height>image->height)
			continue;
		if(rect.width*rect.height<100)
			continue;
		rect_vector.push_back(rect);

	}
	
	if(rect_vector.size()>1)
	{
		rect_fusion(rect_vector,image);
	}
	cvReleaseMemStorage(&siln_contours_storage);
	cvReleaseImage(&silh);
	if(rect_vector.size()>0)
	{
		has_rect = true;
	}
   return has_rect;
}



int BlackboardDetecting::cmp_area(const Rect & a, const Rect & b)
{
	return (a.width * a.height > b.width * b.height);
}


Rect BlackboardDetecting::sort_rect(Rect a, Rect b)
{
	Rect rect_new;
	int small_x = a.x;
	int small_y = a.y;
	int big_x = a.x + a.width;
	int big_y = a.y + a.height;
	if (b.x < small_x)
		small_x = b.x;
	if (b.y < small_y)
		small_y = b.y;
	if (b.x + b.width > big_x)
		big_x = b.x + b.width;
	if (b.y + b.height > big_y)
		big_y = b.y + b.height;
	rect_new = Rect(small_x, small_y, big_x - small_x, big_y - small_y);
	return rect_new;
}




void BlackboardDetecting::rect_fusion2(vector < Rect > &seq, double interval)
{

	std::sort(seq.begin(), seq.end(), cmp_area);
	Rect rect_i;
	Rect rect_j;
	int num = 0;		
	for (;;) {
		std::vector < cv::Rect >::iterator it1;
		std::vector < cv::Rect >::iterator it2;
		num = 0;
		for (it1 = seq.begin(); it1 != seq.end();) {
			for (it2 = it1 + 1; it2 != seq.end();) {
				rect_i = *it1;
				rect_j = *it2;
				if (rect_i.x >
				    (rect_j.x + rect_j.width + interval)
				    || (rect_i.x + rect_i.width + interval) <
				    rect_j.x
				    || rect_i.y >
				    (rect_j.y + rect_j.height + interval)
				    || (rect_i.y + rect_i.height + interval) <
				    rect_j.y) {
					it2++;
					continue;
				} else	
				{
					*it1 = sort_rect(rect_i, rect_j);
					it2 = seq.erase(it2);
					num++;
				}
			}
			it1++;
		}
		
		if (num == 0) {
			
			std::sort(seq.begin(), seq.end(), cmp_area);
			break;
		}

	}

}


std::vector < Rect > BlackboardDetecting::refineSegments2(Mat img, Mat & mask,
						       Mat & dst,
						       double interval,double minarea,double maxarea)
{
	vector < Rect > rect;
	int niters = 2;
	vector < vector < Point > >contours;
	vector < vector < Point > >contours_temp;
	vector < Vec4i > hierarchy;
	vector < Rect > right_rect;
	Mat temp;
	/*dilate(mask, temp, Mat(), Point(-1,-1), 1);
	   erode(temp, temp, Mat(), Point(-1,-1), niters);
	   dilate(temp, temp, Mat(), Point(-1,-1), niters); */

	cv::dilate(mask, temp, cv::Mat());
	cv::erode(mask, temp, cv::Mat());
	cv::erode(mask, temp, cv::Mat());
	cv::dilate(mask, temp, cv::Mat(), cv::Point(-1, -1), 2);

	
	findContours(temp, contours, hierarchy, CV_RETR_EXTERNAL,
		     CV_CHAIN_APPROX_SIMPLE);
	dst = Mat::zeros(img.size(), CV_8UC3);
	if (contours.size() > 0) {
		Scalar color(255, 255, 255);
		for (int idx = 0; idx < contours.size(); idx++) {
			const vector < Point > &c = contours[idx];
			Rect t = boundingRect(Mat(c));
			double area = fabs(contourArea(Mat(c)));
			if (area >= min_area && area<=maxarea)
			{
				contours_temp.push_back(contours[idx]);
				right_rect.push_back(t);
			}
		}
		drawContours(dst, contours_temp, -1, color, CV_FILLED, 8);
	}
    if (right_rect.size() > 1) {
		rect_fusion2(right_rect, interval);
	}
	return right_rect;

}


bool BlackboardDetecting::one_frame_bd(Mat img, vector < Rect > &r)
{
	bool has_rect = false;
	//Mat Img(image);
	//Mat img = Img.clone();
	Mat fgmask;
	Mat fgimg;

	bg_model(img, fgmask, learning_rate);	//update_bg_model ? -1 : 0
	//Mat bg;//\BB\F1\B5õı\B3\BE\B0ͼ\CF\F1
	//bg_model.getBackgroundImage(bg);
	cv::threshold(fgmask, fgmask, mog_threshold, 256, 0);
	r=refineSegments2(img, fgmask, fgimg,merge_interval,min_area,max_area);
	if(r.size()>0)
	{has_rect = true;}
	return has_rect;
}



bool BlackboardDetecting::build_mask_internal(const char *key, Mat& img)
{
	bool masked = false;
	const char *pts = cfg_->get_value(key, "0");
	std::vector < Point > points;
	if (pts) {
		char *data = strdup(pts);
		char *p = strtok(data, ";");
		while (p) {
			
			int x, y;
			if (sscanf(p, "%d,%d", &x, &y) == 2) {
				CvPoint pt = { x, y };
				pt.x = pt.x-masked_rect.x; pt.y = pt.y-masked_rect.y;
				points.push_back(pt);
			}

			p = strtok(0, ";");
		}
		free(data);
	}

	if (points.size() > 3) {
		int n = points.size();
		const Point **pts =
		    (const Point **) alloca(sizeof(const Point *) * points.size());
		for (int i = 0; i < n; i++) {
			pts[i] = &points[i];
		}
		fillPoly(img, pts, &n, 1, CV_RGB(255, 255, 255));
		masked = true;
	}

	return masked;
}


Mat BlackboardDetecting::build_mask(const char *key, const char *key2)
{
	
	CvSize size = {masked_rect.width, masked_rect.height};
	Mat img; img.create(size,CV_8UC3);

	if (!ismask_)
		img.setTo(0);

	if (key) {
		ismask_ = build_mask_internal(key, img);
	}

	if (key2) {
		build_mask_internal(key2, img);
	}
	return img;
}


void BlackboardDetecting::do_mask(Mat &img)
{
	if (ismask_)		
	{
		bitwise_and(img, img_mask_, img);
	} 
}


//bool BlackboardDetecting::build_mask_internal(const char *key, Mat& img)
//{
//	bool masked = false;
//
//	const char *pts = cfg_->get_value(key, "0");
//	std::vector < Point > points;
//
//	if (pts) {
//		char *data = strdup(pts);
//		char *p = strtok(data, ";");
//		while (p) {
//			// ÿ\B8\F6Point ʹ"x,y" \B8\F1ʽ ;
//			int x, y;
//			if (sscanf(p, "%d,%d", &x, &y) == 2) {
//				CvPoint pt = { x, y };
//				points.push_back(pt);
//			}
//
//			p = strtok(0, ";");
//		}
//		free(data);
//	}
//
//	if (points.size() > 3) {
//		int n = points.size();
//		const Point **pts =
//		    (const Point **) alloca(sizeof(const Point *) * points.size());
//		for (int i = 0; i < n; i++) {
//			pts[i] = &points[i];
//		}
//		fillPoly(img, pts, &n, 1, CV_RGB(255, 255, 255));
//		masked = true;
//	}
//
//	return masked;
//}
//

//Mat BlackboardDetecting::build_mask(const char *key, const char *key2)
//{
//	//CvSize size = {video_width_, video_height_};
//	Size size(video_width_, video_height_);
//	Mat img; img.create(size,CV_8UC3);
//
//	if (!ismask_)
//		img.setTo(0);
//
//	if (key) {
//		ismask_ = build_mask_internal(key, img);
//	}
//
//	if (key2) {
//		build_mask_internal(key2, img);
//	}
//	return img;
//}
//

//void BlackboardDetecting::do_mask(Mat &img)
//{
//	if (ismask_)		//\CAǷ\F1\CC\EE\B3\E4\CD\EA\B1\CF ;
//	{
//		bitwise_and(img, img_mask_, img);
//	} 
//}


std::vector<cv::Point> BlackboardDetecting::load_roi(const char *pts)
{
	std::vector<cv::Point> points;
	/*if (!pts) {
		points.push_back(cv::Point(0, 0));
		points.push_back(cv::Point(0, atoi(cfg_->get_value("video_width", "960"))));
		points.push_back(cv::Point(atoi(cfg_->get_value("video_width", "960")), 
			atoi(cfg_->get_value("video_height", "540"))));
		points.push_back(cv::Point(atoi(cfg_->get_value("video_width", "960")), 0));
		return points;
	}*/
	char key[64];
	if (pts) 
	{
		char *data = strdup(pts);
		char *p = strtok(data, ";");
		while (p) 
		{
			// ÿ\B8\F6Point ʹ"x,y" \B8\F1ʽ;
			int x, y;
			if (sscanf(p, "%d,%d", &x, &y) == 2) 
			{
				CvPoint pt = { x, y };
				points.push_back(pt);
			}

			p = strtok(0, ";");
		}
		free(data);
	}

	return points;
}


int BlackboardDetecting::cmp_min_x(const Point & a, const Point & b)
{
	return (a.x < b.x);
}



int BlackboardDetecting::cmp_min_y(const Point & a, const Point & b)
{
	return (a.y < b.y);
}



cv::Rect BlackboardDetecting::get_point_rect(std::vector<cv::Point> pt)
{
	cv::Rect rect;
	int min_x,min_y;
	int max_x,max_y;
	std::sort(pt.begin(), pt.end(), cmp_min_x);
	min_x = pt[0].x;
	max_x = pt[pt.size()-1].x;
	std::sort(pt.begin(), pt.end(), cmp_min_y);
	min_y = pt[0].y;
	max_y = pt[pt.size()-1].y;
	rect = Rect(min_x,min_y,(max_x-min_x),(max_y-min_y));
	rect &= Rect(0,0,video_width_,video_height_);
	return rect;
}


cv::Rect BlackboardDetecting::get_rect(const char*cb_date,const char*cb_date_2)
{
	Rect masked;
	std::vector<cv::Point> pt_vector,pt_vector_1,pt_vector_2;
	if(cb_date) 
	{
		pt_vector_1 = load_roi(cb_date);
		if(pt_vector_1.size()>2) 
		{
			for(int i = 0;i<pt_vector_1.size();i++)
			{
				pt_vector.push_back(pt_vector_1[i]);
			}
		}
	}	
	if(cb_date_2)
	{
		pt_vector_2 = load_roi(cb_date_2);
		if(pt_vector_2.size()>2) 
		{
			for(int i = 0;i<pt_vector_2.size();i++)
			{
				pt_vector.push_back(pt_vector_2[i]);
			}
		}
	}	
	if(pt_vector_1.size()<3 && pt_vector_2.size()<3)
	{
		masked = Rect(0,0,atoi(cfg_->get_value("video_width", "960")),atoi(cfg_->get_value("video_height", "540")));
	}
	else
	{
		masked = get_point_rect(pt_vector);
	}
	return masked;
}
