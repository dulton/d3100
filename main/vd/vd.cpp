#include "../libopencv_detect_teacher/hi_mat.h"

struct RECT {
	int x;
	int y;
	int rows;
	int cols;
};

struct vd_t
{
	hiMat mat;	
};

static int get_b_num(hiMat *mat, RECT rc, int bv)
{
	int sum = 0;
	int stride = mat->get_stride();
	unsigned char *ptr = mat->get_vir_addr()
		+ rc.x;
	for (int i = 0; i < rc.rows; i++) {
		for (int j = 0; j < rc.cols; j++) {
			if (*(ptr + i) == 1)
				sum += 1;
		}
		ptr = ptr + stride
	}
}

static hiMat get_mat(VIDEO_FRAME_INFO_S *frame)
{
	int width = curr_vga_frame->stVFrame.u32Width;
	int height = curr_vga_frame->stVFrame.u32Height;
	int stride = curr_vga_frame->stVFrame.U32Stride[0];
	int phy_addr = curr_vga_fram->stVFrame.u32PhyAddr[0];
	hiMat mat(phy_addr, width, height, stride, hiMat::SINGLE);
	return mat;
}

vd_t* vd_open()
{
	return new vd_t;
}

int vd_changed(vd_t *vd,  VIDEO_FRAME_INFO_S *curr_vga_frame);
{
	if (vd->mat.get_vir_addr ==0) {
		vd->mat = getmat(curr_vga_frame);
		vd->mat.clone();	
		return 0;
	}

	hiMat d_mat, t_mat;
	mat = get_mat(curr_vga_frame);	
	hi::absdiff(&vd->mat, mat, &d_mat);
	hi::threshold(&res, &t_mat, 50, 1);
	RECT rect = {20, 30, 40, 60};	
	int num = get_b_num(&t_mat, rect);
	vd->mat = mat;
	vd->mat.clone();
	if ((double)num / (rect.width * rect.height)> 0.6)
		return 1;
	return 0;
}

void* vd_close(vd_t *vd)
{
	delete vd;
}
