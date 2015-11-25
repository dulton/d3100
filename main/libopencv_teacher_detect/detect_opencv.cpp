
#include "../libteacher_detect/detect.h"
#include "detect_t.h"
#include"blackboard_detect.h"
#include <string>
#include <sstream>
#include "sys/timeb.h"
#include "vi_src.h"
#include <unistd.h>
#include "utils.h"

struct detect_t {
	KVConfig *cfg_;
	TeacherDetecting *detect_;
	BlackboardDetecting *bd_detect_;
	IplImage *masked_;
	bool t_m;
	bool b_m;

	 std::string result_str;

	visrc_t *src;
};

detect_t *det_open(const char *cfg_name)
{
	detect_t *ctx = new detect_t;
	ctx->cfg_ = new KVConfig(cfg_name);

	//++++++++++
	ctx->t_m = false;
	ctx->b_m = false;

	const char *method =
	    ctx->cfg_->get_value("BLACKBOARD_OR_TEACHER", "teacher");
	fprintf(stderr, "======================> method=%s\n", method);

	if (strcmp(method, "teacher") == 0) {
		ctx->t_m = true;
		ctx->detect_ = new TeacherDetecting(ctx->cfg_);
		fprintf(stderr, "INFO: using Teacher Mode...\n");
	} else if (strcmp(method, "blackboard") == 0) {
		ctx->b_m = true;
		ctx->bd_detect_ = new BlackboardDetecting(ctx->cfg_);	//+++++++;
	}
	ctx->src = vs_open(cfg_name);
	//++++++++++
	return ctx;
}

void det_close(detect_t * ctx)
{
	delete ctx->cfg_;
	//++++++++;
	if (ctx->t_m) {
		delete ctx->detect_;
	} else if (ctx->b_m) {
		delete ctx->bd_detect_;
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

static const char *det_detect(detect_t * ctx, Mat & img)
{
	static size_t _cnt = 0;
	fprintf(stderr, "DEBUG: #%u\t", _cnt++);

	char *str = (char *)alloca(BUFSIZE);
	bool isrect = false;
	std::vector < Rect > r;
	vector < cv::Rect > first_r;
	Mat Img = img.clone();

	if (ctx->t_m) {
		Mat masked_img_temp = Mat(Img, ctx->detect_->masked_rect);
		Mat masked_img;

		masked_img_temp.copyTo(masked_img);
		ctx->detect_->do_mask(masked_img);

		isrect = ctx->detect_->one_frame_luv(Img, masked_img, r, first_r);

		//save_mat(img, "saved/origin.rgb");
		save_mat(masked_img, "saved/curr.rgb");
		
		if (isrect) {
			fprintf(stderr, "DEBUG: found target: !!!!\n");
		}

		vector_to_json_t(r, cv::Rect(), false, isrect, str);      
		
		//***********************************
		//***************调试****************
		if (atoi(ctx->cfg_->get_value("debug", "0")) > 0) {
			for (int i = 0; i < r.size(); i++) {
				rectangle(Img, r[i], Scalar(0, 0, 255), 2);
			}
			for (int i = 0; i < first_r.size(); i++) {
				cv::Rect box = ctx->detect_->masked_rect;
				first_r[i].x = first_r[i].x + box.x;
				first_r[i].y = first_r[i].y + box.y - 20;
				first_r[i].height =
				    first_r[i].height + box.y + 40;
				first_r[i] &=
				    cv::Rect(0, 0, Img.cols, Img.rows);
				rectangle(Img, first_r[i], Scalar(255, 0, 0),
					  2);
			}
			//rectangle(Img, upbody, Scalar(255, 255, 255), 2);
		}

	}
	ctx->result_str = str;

	return ctx->result_str.c_str();
}

static const char *empty_result()
{
	return "{\"stamp\":12345, \"rect\":[]}";
}

static bool next_frame(detect_t * ctx, cv::Mat & frame)
{
	/** TODO: 从vi得到下一帧图像 ...
	 */
	return vs_next_frame(ctx->src, frame);
}

const char *det_detect(detect_t * ctx)
{
	cv::Mat frame;
	if (next_frame(ctx, frame)) {
		return det_detect(ctx, frame);
	} else {
		return empty_result();
	}
}
