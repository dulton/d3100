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

	IVE_SRC_INFO_S src_info = get_src_info_s(src, IVE_SRC_FMT_SP420);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	s32Ret = HI_MPI_IVE_DILATE(&IveHandle, &src_info, &dst_mem_info, &pstDilateCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%s\n", __FILE__, __LINE__);
		exit(-1);
	}

}

} // namespace hi

