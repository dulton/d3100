#include "../libteacher_detect/detect.h"
#include <string>
#include <sstream>
#include "sys/timeb.h"
#include <unistd.h>
#include "detect_t.h"
//#include "blackboard_detect.h"
#include "utils.h"
#include "hi_mat.h"
#include "vi_src.h"

#define max(a,b) (a>b ? a:b)
#define min(a,b) (a>b ? b:a)

struct detect_t {
	KVConfig *cfg_;
	TeacherDetecting *detect_;
//	BlackboardDetecting *bd_detect_;
	IplImage *masked_;
	bool t_m;
	bool b_m;
	std::string result_str;

	visrc_t *src;
};

static hiMat *get_mat(const struct hiVIDEO_FRAME_INFO_S *curr_vga_frame)
{
	int width = curr_vga_frame->stVFrame.u32Width;
	int height = curr_vga_frame->stVFrame.u32Height;
	int stride = curr_vga_frame->stVFrame.u32Stride[0];
	int phy_addr = curr_vga_frame->stVFrame.u32PhyAddr[0];

	return new hiMat(phy_addr, width, height, stride, hiMat::SP420); 
}

detect_t *det_open(const char *cfg_name)
{
	detect_t *ctx = new detect_t;
	ctx->cfg_ = new KVConfig(cfg_name);

	//++++++++++
	ctx->t_m = false;
	ctx->b_m = false;

	const char *method =
	    ctx->cfg_->get_value("BLACKBOARD_OR_TEACHER", "teacher");

	if (strcmp(method, "teacher") == 0) {
		ctx->t_m = true;
		ctx->detect_ = new TeacherDetecting(ctx->cfg_);
	} else if (strcmp(method, "blackboard") == 0) {
		ctx->b_m = true;
//		ctx->bd_detect_ = new BlackboardDetecting(ctx->cfg_);	//+++++++;
	}
	//++++++++++
	ctx->src = vs_open(cfg_name);
	return ctx;
}

void det_close(detect_t * ctx)
{
	delete ctx->cfg_;
	//++++++++;
	if (ctx->t_m) {
		delete ctx->detect_;
	} else if (ctx->b_m) {
//		delete ctx->bd_detect_;
	}
	//+++++++;
	vs_close(ctx->src);
	delete ctx;
}

void vector_to_json_t(std::vector < Rect > r, cv::Rect upbody_rect,
		      bool is_upbody, bool is_rect, char *buf)
{
	int offset = 0;
	offset = sprintf(buf, "{\"stamp\":%d,", time(0));
	if (!is_rect) {
		offset += sprintf(buf + offset, "\"rect\":[ ],");
	} else {
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
	if (!is_upbody) {
		offset +=
		    sprintf(buf + offset,
			    "\"up_rect\":{\"x\":0,\"y\":0,\"width\":0,\"height\":0}");
	} else {
		offset += sprintf(buf + offset, "\"up_rect\":");
		offset += sprintf(buf + offset,
				  "{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
				  upbody_rect.x, upbody_rect.y,
				  upbody_rect.width, upbody_rect.height);
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

#define BUFSIZE 400

/** save Mat data, 应该无法支持 32F 之类的，但足够了 */
static void save_mat(const cv::Mat &m, const char *fname)
{
	fprintf(stderr, "DEBUG: m: (%d,%d)\n", m.cols, m.rows);
	FILE *fp = fopen(fname, "wb");
	if (fp) {
		for (int y = 0; y < m.rows; y++) {
			const unsigned char *p = m.ptr<unsigned char>(y);
			fwrite(p, 1, m.elemSize() * m.cols, fp);
		}

		fclose(fp);
	}
}

static const char *det_detect(detect_t * ctx, cv::Mat & img)
{
	static size_t _cnt = 0;
	char *str = (char *)alloca(BUFSIZE);
	bool isrect = false;
	std::vector < Rect > r;
	vector < cv::Rect > first_r;

	//Mat Img = img.clone();

	if (ctx->t_m) {
	/*	Mat masked_img_temp = Mat(Img, ctx->detect_->masked_rect);
		Mat masked_img;
		masked_img_temp.copyTo(masked_img);*/

		Mat masked_img_temp = Mat(img, ctx->detect_->masked_rect);
		ctx->detect_->do_mask(masked_img_temp);

		Mat masked_img;
                masked_img.create(max(64,masked_img_temp.rows), 
				  (max(64,masked_img_temp.cols) + (max(64,masked_img_temp.cols) % 2)), CV_8UC3);
		masked_img.setTo(Scalar(0,0,0));

		masked_img_temp.copyTo(masked_img(cv::Rect(0,0,masked_img_temp.cols,masked_img_temp.rows)));

		//isrect = ctx->detect_->one_frame_luv(Img, masked_img, r, first_r);
		isrect = ctx->detect_->one_frame_luv(img, masked_img, r, first_r);

		//save_mat(img, "saved/origin.rgb");
		//save_mat(masked_img, "saved/curr.rgb");
		for (int i = 0; i < r.size(); i++) 
		{
			cv::Rect box = ctx->detect_->masked_rect;
			r[i].x = r[i].x + box.x;
			r[i].y = r[i].y + box.y;
		}
		vector_to_json_t(r, cv::Rect(), false, isrect, str);      
		
		//***********************************
		//***************调试****************
		if (atoi(ctx->cfg_->get_value("debug", "0")) > 0) {
			for (int i = 0; i < r.size(); i++) {
				//rectangle(Img, r[i], Scalar(0, 0, 255), 2);
				rectangle(img, r[i], Scalar(0, 0, 255), 2);
			}
			for (int i = 0; i < first_r.size(); i++) {
				cv::Rect box = ctx->detect_->masked_rect;
				first_r[i].x = first_r[i].x + box.x;
				first_r[i].y = first_r[i].y + box.y - 20;
				first_r[i].height =
				    first_r[i].height + box.y + 40;
				first_r[i] &=
				    cv::Rect(0, 0, img.cols, img.rows);
				//rectangle(Img, first_r[i], Scalar(255, 0, 0), 2);
				rectangle(img, first_r[i], Scalar(255, 0, 0), 2);
			}

		}

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
	return vs_next_frame(ctx->src, frame);
}

const char *det_detect(detect_t * ctx)
{
	cv::Mat frame;
	if (next_frame(ctx, frame)) {
		return det_detect(ctx, frame);	
	}
	else {
		return empty_result();
	}
}

const char *det_detect_vt(detect_t *det, const struct hiVIDEO_FRAME_INFO_S *frame)
{
	fprintf(stderr, "%s %d\n", __func__, __LINE__);
	hiMat *himat = get_mat(frame);  
	hiMat rgb;
	hi::yuv2rgb(*himat, rgb);
	fprintf(stderr, "%s %d\n", __func__, __LINE__);
	delete himat;
	cv::Mat	mat;
	fprintf(stderr, "%s %d\n", __func__, __LINE__);
	rgb.dump_data("./liuwenwen");
	fprintf(stderr , "%s %d\n", __func__, __LINE__);
	rgb.download(mat);
	const char *rc = det_detect(det, mat); 
	return rc;
}
