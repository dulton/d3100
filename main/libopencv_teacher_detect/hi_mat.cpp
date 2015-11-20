#include <stdio.h>
#include "hi_mat.h"
#include "hi_comm_ive.h"
#include "mpi_ive.h"

/** 应该编写 hlp_xxx 用了alloc, copy, free ...

 */
void hlp_alloc(unsigned int *phy_addr_p, void** vir_addr_p, int length)
{

	int rc = HI_MPI_SYS_MmzAlloc_Cached(phy_addr_p, vir_addr_p
					    "User", 0, length);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}

}

hiMat::hiMat()
	: using_ref_(false)
{
}

hiMat::hiMat(const cv::Mat &m)
	: using_ref_(true)
{
	ref = new int;
}

hiMat::~hiMat()
{
	*ref_--;
	if (*ref_ == 0) {
		// TODO: free mem
		delete ref_;
	}
}

void hiMat::download(cv::Mat &m)
{
	
}

void hiMat::create(int rows, int cols, int type)
{
	this.rows = rows;
	this.cols = cols;

	switch (type)
	{
		case 0:
			this.stride_ = (cols + 3) / 4 * 4;
			int len = rows * cols;
			hlp_alloc(&this->phy_addr_, &this->vir_addr_
					, len);
			this->using_ref_ = true;
			*(this->ref_) = 1;
			break;

		case 1:
			this.stride_ = (cols +3) / 4 * 4;
			int len = rows * cols * 3 / 2;
			hlp_alloc(&this->phy_addr_, &this->vir_addr_
					, len);
			this->using_ref_ = true;
			*(this->ref_) = 1;
			break;	

		case 2:
			break;
		default:
			break;
	}
}

hiMat &hiMat::operator=(const hiMat &m)
{
	this->rows = m.rows();
	this->cols = m.cols;
	this->stride = m.get_stride();
	
	this->phy_addr_ = m.get_phy_addr();
	this->vir_addr_ = m.get_vir_addr();
	this->using_ref_ = m.using_ref_;
	this->ref_ = m.ref_;
	this->ref_++;

	return *this;
}

hiMat &hiMat::operator = (const cv::Mat &m)
{
		
	return *this;
}

int hiMat::get_stride() const
{
	return stride_;
}

unsigned int hiMat::get_phy_addr() const
{
	return phy_addr_;
}

void *hiMat::get_vir_addr() const
{
	return vir_addr_;
}

static IVE_MEM_INFO_S get_mem_info_s(const hiMat &src)
{
	IVE_MEM_INFO_S mem_info;

	mem_info.u32PhyAddr = src.get_phy_addr();
	mem_info.u32Stride = src.get_stride();

	return mem_info;
}

static IVE_SRC_INFO_S get_src_info_s(const hiMat &src, IVE_SRC_FMT_E fmt)
{
	IVE_SRC_INFO_S src_info;
	src_info.stSrcMem = get_mem_info_s(src);
	src_info.u32Height = src.rows;
	src_info.u32Width = src.cols;
	src_info.enSrcFmt = fmt;

	return src_info;
}

///////////////////// 以下为对 hiMat 的操作 ////////////////////////
namespace hi
{
	// 建议所有函数都放到 hi 名字空间中 ...
void dilate(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_DILATE_CTRL_S pstDilateCtrl;
	pstDilateCtrl.au8Mask[0] = 255;
	pstDilateCtrl.au8Mask[1] = 255;
	pstDilateCtrl.au8Mask[2] = 255;
	pstDilateCtrl.au8Mask[3] = 255;
	pstDilateCtrl.au8Mask[4] = 255;
	pstDilateCtrl.au8Mask[5] = 255;
	pstDilateCtrl.au8Mask[6] = 255;
	pstDilateCtrl.au8Mask[7] = 255;
	pstDilateCtrl.au8Mask[8] = 255;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SINGLE);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_DILATE(&IveHandle, &src_info, &dst_mem_info, &pstDilateCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

void erode(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_ERODE_CTRL_S pstErodeCtrl;
	pstErodeCtrl.au8Mask[0] = 255;
	pstErodeCtrl.au8Mask[1] = 255;
	pstErodeCtrl.au8Mask[2] = 255;
	pstErodeCtrl.au8Mask[3] = 255;
	pstErodeCtrl.au8Mask[4] = 255;
	pstErodeCtrl.au8Mask[5] = 255;
	pstErodeCtrl.au8Mask[6] = 255;
	pstErodeCtrl.au8Mask[7] = 255;
	pstErodeCtrl.au8Mask[8] = 255;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SINGLE);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_ERODE(&IveHandle, &src_info, &dst_mem_info, &pstErodeCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

void filter(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_FILTER_CTRL_S pstFilterCtrl;
	pstFilterCtrl.u8Norm = 3;
	pstFilterCtrl.as8Mask[0] = 1;
	pstFilterCtrl.as8Mask[1] = 1;
	pstFilterCtrl.as8Mask[2] = 1;
	pstFilterCtrl.as8Mask[3] = 1;
	pstFilterCtrl.as8Mask[4] = 0;
	pstFilterCtrl.as8Mask[5] = 1;
	pstFilterCtrl.as8Mask[6] = 1;
	pstFilterCtrl.as8Mask[7] = 1;
	pstFilterCtrl.as8Mask[8] = 1;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SINGLE);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_FILTER(&IveHandle, &src_info, &dst_mem_info, &pstFilterCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

// 这个阈值时不定值 ....???????...
void threshold(const hiMat &src, hiMat &dst, unsigned int threshold, unsigned int max_value, unsigned int threshold_type = 0)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_THRESH_CTRL_S pstThreshCtrl;
	pstThreshCtrl.enOutFmt = threshold_type; // IVE_THRESH_OUT_FMT_BINARY;
	pstThreshCtrl.u32MaxVal = max_value; // 255;
	pstThreshCtrl.u32MinVal = 0;
	pstThreshCtrl.u32Thresh = threshold; // 55/22;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SINGLE);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_THRESH(&IveHandle, &src_info, &dst_mem_info, &pstThreshCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

void sub(const hiMat &src1, const hiMat &src2, hiMat &dst)
{
	int s32Ret;

	dst.create(src1.cols, src1.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_SUB_OUT_FMT_E enOutFmt;
	enOutFmt = IVE_SUB_OUT_FMT_ABS;

	IVE_SRC_INFO_S src_info1 = get_src_info_s(src1, IVE_SRC_FMT_SINGLE);
	IVE_SRC_INFO_S src_info2 = get_src_info_s(src2, IVE_SRC_FMT_SINGLE);

	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_SUB(&IveHandle, &src_info1, &src_info2, &dst_mem_info, enOutFmt, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

void or(const hiMat &src1, const hiMat &src2, hiMat &dst)
{
	int s32Ret;

	dst.create(src1.cols, src1.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info1 = get_src_info_s(src1, IVE_SRC_FMT_SINGLE);
	IVE_SRC_INFO_S src_info2 = get_src_info_s(src2, IVE_SRC_FMT_SINGLE);

	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_OR(&IveHandle, &src_info1, &src_info2, &dst_mem_info, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

void yuv2rgb(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC3); // hiMat 负责处理失败情况 ...

	IVE_CSC_CTRL_S pstCscCtrl;
	pstCscCtrl.enOutFmt = IVE_CSC_OUT_FMT_PACKAGE;
	pstCscCtrl.enCscMode = IVE_CSC_MODE_VIDEO_BT601_AND_BT656;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SP420);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_THRESH(&IveHandle, &src_info, &dst_mem_info, &pstCscCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

} // namespace hi

