
#include "../libteacher_detect/detect.h"
#include "detect_t.h"
#include"blackboard_detect.h"
#include <string>
#include <sstream>
#include "sys/timeb.h"

struct detect_t
{
	KVConfig *cfg_;
	TeacherDetecting *detect_;
	BlackboardDetecting *bd_detect_;
	IplImage *masked_;
	bool t_m; 
	bool b_m;

	std::string result_str;	
};

detect_t *det_open(const char *cfg_name)
{
	detect_t *ctx = new detect_t;
	ctx->cfg_ = new KVConfig(cfg_name);

	//++++++++++
	ctx->t_m = false; 
	ctx->b_m = false;

	const char *method = ctx->cfg_->get_value("BLACKBOARD_OR_TEACHER", "teacher");
	fprintf(stderr, "======================> method=%s\n", method);

	if (strcmp(method, "teacher") == 0) {
		ctx->t_m = true;
		ctx->detect_ = new TeacherDetecting(ctx->cfg_);
	}	
	else if (strcmp(method, "blackboard") == 0) {
		ctx->b_m = true;
		ctx->bd_detect_ = new BlackboardDetecting(ctx->cfg_);//+++++++;
	}
	//++++++++++
	return ctx;
}

void det_close(detect_t * ctx)
{
	delete ctx->cfg_;
	//++++++++;
	if(ctx->t_m ) { delete ctx->detect_; }
	else if(ctx->b_m ) { delete ctx->bd_detect_; }
	//+++++++;
	delete ctx;
}

void vector_to_json_t(std::vector < Rect > r, cv::Rect upbody_rect,bool is_upbody,bool is_rect, char *buf)
{
	int offset = 0;
	offset = sprintf(buf, "{\"stamp\":%d,", time(0));
	if(!is_rect)	
	{
		offset += sprintf(buf + offset, "\"rect\":[ ],");
	}
	else
	{
		offset += sprintf(buf + offset, "\"rect\":[");
		for (int i = 0; i < r.size(); i++) {
			Rect t = r[i];
			if (i == 0)
				offset +=
					sprintf(buf + offset,
						"{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
						t.x, t.y, t.width, t.height);
			else
				offset +=
					sprintf(buf + offset,
						",{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
						t.x, t.y, t.width, t.height);
		}
		offset += sprintf(buf + offset, "],");
	}	
	if(!is_upbody)
	{
		offset += sprintf(buf + offset, "\"up_rect\":{\"x\":0,\"y\":0,\"width\":0,\"height\":0}");
	}
	else
	{
		offset += sprintf(buf + offset, "\"up_rect\":");
	    offset += sprintf(buf + offset,
				    "{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
				    upbody_rect.x, upbody_rect.y, upbody_rect.width, upbody_rect.height);
	}
	strcat(buf, "}");
}


void vector_to_json(std::vector < Rect > r, char *buf)
{
	int offset = 0;
	offset = sprintf(buf, "{\"stamp\":%d,", time(0));
	offset += sprintf(buf + offset, "\"rect\":[");
	for (int i = 0; i < r.size(); i++) {
		Rect t = r[i];
		if (i == 0)
			offset +=
			    sprintf(buf + offset,
				    "{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
				    t.x, t.y, t.width, t.height);
		else
			offset +=
			    sprintf(buf + offset,
				    ",{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
				    t.x, t.y, t.width, t.height);
	}
	strcat(buf, "]}");

}

#define BUFSIZE 100

static const char* det_detect(detect_t * ctx, Mat &img)
{
	char *str = (char*)alloca(BUFSIZE);
	bool isrect = false;
	std::vector < Rect > r;	
	vector < cv::Rect > first_r;
	timeb pre,cur;
	ftime(&pre);
	Mat Img = img.clone();
	Mat img_t = img.clone();
	ftime(&cur);
	double time = (cur.time-pre.time)*1000+(cur.millitm-pre.millitm);
	fprintf(stderr,"clone time:%f\n",time);
//	blur(img_t,img_t,Size(3,3));
	if(ctx->t_m) 
	{	
		Mat masked_img_temp = Mat(Img, ctx->detect_->masked_rect);	
		Mat masked_img;
		masked_img_temp.copyTo(masked_img);
		ctx->detect_->do_mask(masked_img);		
		isrect = ctx->detect_->one_frame_luv(Img,masked_img, r, first_r);
		for (int i = 0; i<r.size(); i++) {
				cv::Rect box = ctx->detect_->masked_rect;
				r[i].x = r[i].x+box.x;
				r[i].y = r[i].y+box.y;
				r[i] &= cv::Rect(0,0,Img.cols,Img.rows);
		}
	
		cv::Rect upbody;
		bool is_up_body = false;
		if(atoi(ctx->cfg_->get_value("Upbody_Detect", "0"))>0)
		{
			Mat masked_upbody =  Mat(Img, ctx->detect_->upbody_masked_rect);
			ctx->detect_->get_upbody(masked_upbody);	
			if(ctx->detect_->up_update.upbody_rect.size()>0)
			{
				cv::Rect upbody_t = ctx->detect_->up_update.upbody_rect[0];
				if(!(upbody_t.x+upbody_t.width/2>ctx->detect_->upbody_masked_rect.width-15 ||upbody_t.x+upbody_t.width/2<15))
				{
					is_up_body = true;
				        cv::Rect box = ctx->detect_->upbody_masked_rect;
				        upbody = upbody_t;
				        upbody.x = upbody.x+box.x;
					upbody.y = upbody.y+box.y;
					upbody &= cv::Rect(0,0,Img.cols,Img.rows);
				}							
			}
		
		}
//		vector_to_json_t(r, upbody,is_up_body,isrect,str);	
		//***********************************
		//***************调试****************
		if (atoi(ctx->cfg_->get_value("debug", "0")) > 0) 
		{
			for (int i = 0; i<r.size(); i++) {
				rectangle(Img, r[i], Scalar(0, 0, 255), 2);
			}
			for (int i = 0; i < first_r.size(); i++) {
				cv::Rect box = ctx->detect_->masked_rect;
				first_r[i].x = first_r[i].x+box.x;
				first_r[i].y = first_r[i].y+box.y - 20;
				first_r[i].height = first_r[i].height+box.y + 40;
				first_r[i] &= cv::Rect(0,0,Img.cols,Img.rows);
				rectangle(Img, first_r[i], Scalar(255, 0, 0), 2);			}
    			rectangle(Img, upbody, Scalar(255, 255, 255), 2);
		}
			
	}
	//***************************板书探测****************************;
	else if(ctx->b_m)
	{
		 
		Mat masked_img = Mat(Img,ctx->bd_detect_->masked_rect);
		ctx->bd_detect_->do_mask(masked_img); 	
		isrect = ctx->bd_detect_->one_frame_bd(masked_img, r);
		for (int i = 0; i<r.size(); i++) {
			cv::Rect box = ctx->bd_detect_->masked_rect;
			r[i].x = r[i].x+box.x;
			r[i].y = r[i].y+box.y;
			r[i] &= cv::Rect(0,0,Img.cols,Img.rows);
		}
		if (atoi(ctx->cfg_->get_value("debug", "0")) > 0) {
			for (int i = 0; i < r.size(); i++) {
				rectangle(Img, r[i], Scalar(0, 0, 255), 2);
			}
		}
//		if (isrect) 
//		{
//			vector_to_json(r, str);
//		}
//		else 
//		{
//			snprintf(str, BUFSIZE, "{\"stamp\": %d, \"rect\": [ ] }", time(0));
//		}
	}
	ctx->result_str = str;
	return ctx->result_str.c_str();
}

static const char *empty_result()
{
	return "{\"stamp\":12345, \"rect\":[]}";
}

static bool next_frame(detect_t *ctx, cv::Mat &frame)
{
	/** TODO: 从vi得到下一帧图像 ...
	 */
	return false;
}

const char *det_detect(detect_t *ctx)
{
	cv::Mat frame;
	if (next_frame(ctx, frame)) {
		return det_detect(ctx, frame);
	}
	else {
		return empty_result();
	}
}

