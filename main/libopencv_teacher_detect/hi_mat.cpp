#include "config.h"
#include <stdio.h>

#ifdef DEBUG_HIMAT
#	include <stdlib.h>
#	include <string.h>
#else
#	include "hi_comm_sys.h"
#	include "mpi_sys.h"
#	include "hi_comm_ive.h"
#	include "mpi_ive.h"
#endif

#include "hi_mat.h"

/** 应该编写 hlp_xxx 用了alloc, copy, free ...

 */
static void hlp_alloc(unsigned int *phy_addr_p, void** vir_addr_p, int length)
{
#ifdef DEBUG_HIMAT
	*phy_addr_p = 0;
	*vir_addr_p = malloc(length);
	fprintf(stderr, "%s: malloc %p\n", __func__, *vir_addr_p);
#else
	int rc = HI_MPI_SYS_MmzAlloc_Cached(phy_addr_p, vir_addr_p
					    ,"User", 0, length);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}
#endif // DEBUG_HIMAT
}

/** 这是从 Mat 复制数据到 hiMat，
 */
static void hlp_copy(void* vir_addr, size_t hiMat_stride_bytes, const cv::Mat &m)
{
	unsigned char *dst = (unsigned char*)vir_addr;
	for (int i = 0; i < m.rows; i++) {
		const unsigned char *p = m.ptr<unsigned char>(i);
		size_t len = m.elemSize() * m.cols;
		memcpy(dst, p, len);
		dst += hiMat_stride_bytes;
	}
}

static void hlp_free(unsigned int phy, void *vir)
{
#ifdef DEBUG_HIMAT
	free(vir);
	fprintf(stderr, "%s: free %p\n", __func__, vir);
#else
	HI_MPI_SYS_MmzFree(phy, vir);
#endif // DEBUG_HIMAT
}

hiMat::hiMat()
	: ref_(0)
{
	memset(this, 0, sizeof(hiMat));
}

hiMat::hiMat(const cv::Mat &m)
{
	*this = m;	
}

hiMat::hiMat(const hiMat &m)
{
	memset(this, 0, sizeof(hiMat));
	*this = m;
}

hiMat::~hiMat()
{
	release();
}

void hiMat::download(cv::Mat &m)
{
	m.create(rows, cols, CV_8U);
	unsigned char *p = (unsigned char*)vir_addr_;
	for (int i = 0; i < rows; i++) {
		unsigned char *q = m.ptr<unsigned char>(i);
		memcpy(q , p, m.cols);
		p += stride_;
	}

}

void hiMat::release()
{
	if (ref_) {
		if (*ref_ == 0) {
			fprintf(stderr, "FATAL: %s: %s:%d\n",
					__func__, __FILE__, __LINE__);
			exit(-1);
		}
		(*ref_)--;
		if (*ref_ == 0) {
			hlp_free(phy_addr_, vir_addr_);
			delete ref_;
			ref_ = 0;
		}
	}
}

void hiMat::addref()
{
	if (ref_) {
		(*ref_)++;
	}
}

void hiMat::create(int rows, int cols, size_t bytes_of_line)
{
	release();

	this->rows = rows;
	this->cols = cols;
	this->stride_ = (bytes_of_line + 7) / 8 * 8;

	hlp_alloc(&phy_addr_, &vir_addr_, rows * stride_);

	ref_ = new size_t;
	*ref_ = 1;
}

hiMat &hiMat::operator = (const hiMat &m)
{
	// a = a;
	if (this == &m) {
		return *this;
	}

	// release old
	release();

	// cp
	phy_addr_ = m.phy_addr_;
	vir_addr_ = m.vir_addr_;
	cols = m.cols;
	rows = m.rows;
	ref_ = m.ref_;

	// addref
	addref();

	return *this;
}

hiMat &hiMat::operator = (const cv::Mat &m)
{
	create(m.rows, m.cols, m.cols*m.elemSize());	
	hlp_copy(vir_addr_, stride_, m);

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

#ifndef DEBUG_HIMAT

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
void threshold(const hiMat &src, hiMat &dst, unsigned int threshold, unsigned int max_value)
{
	int s32Ret;

	dst.create(src.cols, src.rows, CV_8UC1); // hiMat 负责处理失败情况 ...

	IVE_THRESH_CTRL_S pstThreshCtrl;
	pstThreshCtrl.enOutFmt = IVE_THRESH_OUT_FMT_BINARY; // IVE_THRESH_OUT_FMT_BINARY;
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

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

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

#endif // DEBUG_HIMAT
