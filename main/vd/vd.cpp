#include <stdio.h>
#include <stdint.h>
#define WITHOUT_OCV

#include "../libopencv_teacher_detect/hi_mat.h"
#include "hi_comm_ive.h"
#include "vd.h"

struct vd_t
{
	vd_t() { firsted = true; }

	hiMat mat;	
	bool firsted;
};

/** FRAME to hiMat.clone */
static hiMat get_mat(const struct hiVIDEO_FRAME_INFO_S *curr_vga_frame)
{
	int width = curr_vga_frame->stVFrame.u32Width;
	int height = curr_vga_frame->stVFrame.u32Height;
	int stride = curr_vga_frame->stVFrame.u32Stride[0];
	int phy_addr = curr_vga_frame->stVFrame.u32PhyAddr[0];

	hiMat mat(phy_addr, width, height, stride, hiMat::SINGLE); // only using Y
	return mat.clone();
}

vd_t* vd_open()
{
	return new vd_t;
}

static int get_sub(const hiMat &mat, const cv::Rect &rc)
{
	/** 是对二值图像进行积分，如果要计算面积比值，应该除以 255 
	  	二值图是 0|255
	 */
	mat.flush();
	uint64_t topleft = mat.at<uint64_t>(rc.y, rc.x) & 0xfffffff;
	uint64_t topright = mat.at<uint64_t>(rc.y, rc.x + rc.width) & 0xfffffff;
	uint64_t downleft = mat.at<uint64_t>(rc.y + rc.height, rc.x) & 0xfffffff;
    uint64_t downright = mat.at<uint64_t>(rc.y + rc.height, rc.x + rc.width) & 0xfffffff;

	return (downright + topleft - topright - downleft) / 255;
}

int vd_changed(vd_t *vd, const struct hiVIDEO_FRAME_INFO_S *curr_vga_frame)
{
	if (vd->firsted) {
		vd->firsted = false;
		vd->mat = get_mat(curr_vga_frame);
		return 0;
	}

	/** VGA 检测区域，定义为:
	   	 curr_vga_frame 中的中心，左右去除 100像素，上下去除 50 像素 .
	 */
#define DWIDTH 100
#define DHEIGHT 50

	cv::Rect rect(DWIDTH, DHEIGHT, curr_vga_frame->stVFrame.u32Width - DWIDTH*2, 
			curr_vga_frame->stVFrame.u32Height - DHEIGHT*2);	 // TODO: read from cfg;

	hiMat d_mat, t_mat, i_mat;
	hiMat mat = get_mat(curr_vga_frame);	

	hi::absdiff(vd->mat, mat, d_mat);
	vd->mat = mat;

	hi::threshold(d_mat, t_mat, 50, 255);
	hi::integral(t_mat, i_mat);

	int num = get_sub(i_mat, rect); // 如果 num 比较大，说明有触发，可以根据你那面的实际情况 .
									// 看看 vga 不变时的值，和变化的值，一般是非常明显的 ...
	fprintf(stderr, "%u ", num);

	return 0;
}

void vd_close(vd_t *vd)
{
	delete vd;
}
