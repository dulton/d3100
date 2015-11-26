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
#include "utils.h"

/** 应该编写 hlp_xxx 用了alloc, copy, free ...

 */
static void hlp_alloc(unsigned int *phy_addr_p, void** vir_addr_p, int length)
{
#ifdef DEBUG_HIMAT
	*phy_addr_p = 0;
	*vir_addr_p = malloc(length);
	fprintf(stderr, "%s: malloc %p\n", __func__, *vir_addr_p);
#else
	int rc = HI_MPI_SYS_MmzAlloc_Cached(phy_addr_p, vir_addr_p ,"User", 0, length);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}
#endif // DEBUG_HIMAT
}

/** 这是从 Mat 复制数据到 hiMat，
 */
#ifndef WITHOUT_OCV
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
#endif // WITHOUT opencv

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
{
	memset(this, 0, sizeof(hiMat));
	ref_ = 0;
	outer_ = false;
}

#ifndef WITHOUT_OCV
hiMat::hiMat(const cv::Mat &m)
{
	outer_ = false;
	ref_ = 0;
	*this = m;	
}

hiMat::hiMat(const cv::Mat &m, const cv::Rect &roi)
{
	//fprintf(stderr, "FATAL: %s: NOT impl\n", __func__);
	outer_ = false;
	ref_ = 0;
	*this = m(roi);
}
#endif // without opencv

hiMat::hiMat(const hiMat &m)
{
	memset(this, 0, sizeof(hiMat));
	outer_ = false;
	*this = m;
}

hiMat::hiMat(const hiMat &m, const cv::Rect &roi)
{
	fprintf(stderr, "FATAL: %s: NOT impl\n", __func__);
	// TODO: 实现 roi，需要分两种情况，如果 m 为 outer，则需要 deep cp
	//		 否则使用 addref
}

hiMat::hiMat(unsigned int phyaddr, int width, int height, int stride, Type type)
{
	outer_ = true;
	phy_addr_ = phyaddr;
	this->type = type;
	cols = width;
	rows = height;
	stride_ = stride;
	hi_stride_ = (width + 7) / 8 * 8;
	vir_addr_ = HI_MPI_SYS_Mmap(phy_addr_, memsize());
}

hiMat hiMat::operator()(const cv::Rect &roi) const
{
	return hiMat(*this, roi);
}

hiMat::~hiMat()
{
	if (!outer_)
		release();
	else {
		HI_MPI_SYS_Munmap(vir_addr_, memsize());
	}
}

void hiMat::dump_hdr() const
{
	fprintf(stderr, "DEBUG: m: <%u>: type=%d| (%d-%d), %d|%d, %u, %u, %p\n",
			ref_ ? *ref_ : 0, type, cols, rows, stride_, hi_stride_, memsize(), phy_addr_, vir_addr_);
}

void hiMat::dump_data(const char *fname) const
{
	flush();
	FILE *fp = fopen(fname, "wb");
	if (fp) {
		const unsigned char *s = (unsigned char*)vir_addr_;

		int ds = cols;
		if (type == RGB24)
			ds = cols * 3;

		if (type == U64)
			ds = cols * 8;

		for (int i = 0; i < rows; i++) {
			fwrite(s, 1, ds, fp);
			s += stride_;
		}

		if (type == SP420) {
			// write UV
			for (int i = 0; i < rows / 2; i++) {
				fwrite(s, 1, ds, fp);
				s += stride_;
			}
		}

		fclose(fp);
	}
}

void hiMat::flush() const
{
	HI_MPI_SYS_MmzFlushCache(phy_addr_, vir_addr_, memsize());
}

#ifndef WITHOUT_OCV
void hiMat::download(cv::Mat &m)
{
	/** FIXME: 这里还是需要类型的 ...
	  		先简单的假设，hiMat 对应三种数据格式: 
				rgb24：stride_ / cols >= 3
				sp420: stride_ / cols >= 2 
				gray: stride_ / cols >= 1
	 */

	flush();
	size_t ds = cols;
	if (type == RGB24) {
		m.create(rows, cols, CV_8UC3);	// rgb24
		ds = cols * 3;
	}
	else if (type == SP420) {
		/** FIXME: 此处生成 8UC1 的 m
		 */
		m.create(rows, cols, CV_8UC1);	// gray, 仅仅复制 Y
		ds = cols;
	}
	else if (type == SINGLE) {
		m.create(rows, cols, CV_8U);
		ds = cols;
	}
	else if (type == U64) {
		m.create(rows, cols, CV_32F);
	}
	else {
		fprintf(stderr, "FATAL: hiMat::download NOT impl..\n");
		exit(-1);
	}

	if (type == U64) {
		/** FIXME: 这里仅仅保存 sum 值，输出 mat 为 32FC1 
		 */
		int64_t *p = (int64_t*)vir_addr_;
		for (int i = 0; i < rows; i++) {
			float *q = m.ptr<float>(i);
			for (int j = 0; j < cols; j++) {
				q[j] = p[j] & 0xfffffff;
			}

			unsigned char *x = (unsigned char*)p;
			x += stride();
			p = (int64_t*)x;
		}
	}
	else {
		unsigned char *p = (unsigned char*)vir_addr_;
		for (int i = 0; i < rows; i++) {
			unsigned char *q = m.ptr<unsigned char>(i);
			memcpy(q , p, ds);
			p += stride_;
		}
	}
}
#endif // without opencv

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

hiMat hiMat::clone() const
{
	hiMat m;
	m.create(rows, cols, type);
	deepcp(m);
	return m;
}

void hiMat::deepcp(hiMat &m) const
{
	const char *s = (const char*)vir_addr_;
	char *d = (char*)m.vir_addr_;

	switch (type) {
	case SP420:
		// Y
		for (int i = 0; i < rows; i++) {
			memcpy(d, s, cols);
			s += stride_;
			d += m.stride_;
		}
		// UV
		for (int i = 0; i < rows/2; i++) {
			memcpy(d, s, cols);
			s += stride_;
			d += m.stride_;
		}
		break;

	case SINGLE:
		for (int i = 0; i < rows; i++) {
			memcpy(d, s, cols);
			s += stride_;
			d += m.stride_;
		}
		break;

	case RGB24:
		for (int i = 0; i < rows; i++) {
			memcpy(d, s, cols*3);
			s += stride_;
			d += m.stride_;
		}
		break;

	case U64:
		for (int i = 0; i < rows; i++) {
			memcpy(d, s, cols*8);
			s += stride_;
			d += m.stride_;
		}
		break;

	default:
		fprintf(stderr, "FATAL: %s: unknown type!!!\n", __func__);
		exit(-1);
		break;
	}
}

size_t hiMat::memsize() const
{
	switch (type) {
	case RGB24:
	case SINGLE:
	case U64:
		return rows * stride_;

	case SP420:
		return rows * stride_ * 3 / 2;
	}

	fprintf(stderr, "FATAL: %s: Only support hiMat::Type types!!!\n", __func__);
	return -1;
}

void hiMat::create(int rows, int cols, hiMat::Type type)
{
	if (outer_) {
		HI_MPI_SYS_Munmap(vir_addr_, memsize());
	}
	else {
		release();
	}

	this->rows = rows;
	this->cols = cols;
	this->type = type;

	hi_stride_ = (cols + 7) / 8 * 8;

	switch (type) {
	case RGB24:
		stride_ = hi_stride_ * 3;
		break;

	case SINGLE:
	case SP420:
		stride_ = hi_stride_;
		break;

	case U64:
		stride_ = hi_stride_ * 8;
		break;

	default:
		fprintf(stderr, "FATAL: %s: unknown type of %d\n", __func__, type);
		exit(-1);
		break;
	}	

	hlp_alloc(&phy_addr_, &vir_addr_, memsize());

	ref_ = new size_t;
	*ref_ = 1;
	outer_ = false;
}

hiMat &hiMat::operator = (const hiMat &m)
{
	// a = a;
	if (this == &m) {
		return *this;
	}

	// release old
	if (outer_) {
		HI_MPI_SYS_Munmap(vir_addr_, rows * stride_);
	}
	else {
		release();
	}

	outer_ = false;

	// 如果 m 是 outer_ 对象，则需要 clone
	if (m.outer_) {
		delete ref_;
		ref_ = 0;
		create(m.rows, m.cols, m.type);
		deepcp(*this);
	}
	else {
		// cp
		phy_addr_ = m.phy_addr_;
		vir_addr_ = m.vir_addr_;
		cols = m.cols;
		rows = m.rows;
		ref_ = m.ref_;
		hi_stride_ = m.hi_stride_;
		stride_ = m.stride_;
		type = m.type;

		// addref
		addref();
	}

	return *this;
}

#ifndef WITHOUT_OCV
hiMat &hiMat::operator = (const cv::Mat &m)
{
	/** FIXME: 仅仅支持 8UC1
	 */
	if (m.elemSize() != 1) {
		fprintf(stderr, "FATAL: %s: only support 8UC1 Mat!!!\n", __func__);
		exit(-1);
	}

	create(m.rows, m.cols, SINGLE);
	hlp_copy(vir_addr_, stride_, m);

	return *this;
}
#endif // without opencv

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
	mem_info.u32Stride = src.hi_stride();

	return mem_info;
}

static IVE_SRC_INFO_S get_src_info_s(const hiMat &src)
{
	IVE_SRC_INFO_S src_info;
	src_info.stSrcMem = get_mem_info_s(src);
	src_info.u32Height = src.rows;
	src_info.u32Width = src.cols;
	switch (src.type) {
	case hiMat::SINGLE:
		src_info.enSrcFmt = IVE_SRC_FMT_SINGLE;
		break;

	case hiMat::SP420:
		src_info.enSrcFmt = IVE_SRC_FMT_SP420;
		break;
	}

	return src_info;
}

static void dump_src_info(const IVE_SRC_INFO_S &info)
{
	fprintf(stderr, "DEBUG: == %s\n", __func__);
	fprintf(stderr, "\tphyaddr=%u, stride=%d\n", info.stSrcMem.u32PhyAddr, info.stSrcMem.u32Stride);
	fprintf(stderr, "\twidth=%d, height=%d\n", info.u32Width, info.u32Height);
	fprintf(stderr, "\tfmt=%s\n", info.enSrcFmt == IVE_SRC_FMT_SP420 ? "SP420" : 
			info.enSrcFmt == IVE_SRC_FMT_SINGLE ? "SINGLE" : "UNKNOWN!!!");
}

static void dump_dst_info(const IVE_MEM_INFO_S &info)
{
	fprintf(stderr, "DEBUG: == %s\n", __func__);
	fprintf(stderr, "\tphyaddr=%u, stride=%d\n", info.u32PhyAddr, info.u32Stride);
}

///////////////////// 以下为对 hiMat 的操作 ////////////////////////
namespace hi
{
	// 建议所有函数都放到 hi 名字空间中 ...
// 图像膨胀(源数据只能为单分量) ...
void dilate(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.rows, src.cols, hiMat::SINGLE); // hiMat 负责处理失败情况 ...

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

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src.flush();

	s32Ret = HI_MPI_IVE_DILATE(&IveHandle, &src_info, &dst_mem_info, &pstDilateCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
}

// 图像腐蚀(源数据只能为单分量) ...
void erode(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.rows, src.cols, hiMat::SINGLE); // hiMat 负责处理失败情况 ...

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

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src.flush();

	s32Ret = HI_MPI_IVE_ERODE(&IveHandle, &src_info, &dst_mem_info, &pstErodeCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
}

// 图像滤波(源数据可以是单分量,sp420,sp422) ...
// 当输入为sp420和sp422时宽度必须为偶数 ...
void filter(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.rows, src.cols, src.type);// hiMat 负责处理失败情况 ...

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

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src.flush();

	s32Ret = HI_MPI_IVE_FILTER(&IveHandle, &src_info, &dst_mem_info, &pstFilterCtrl, bInstant);
	if(s32Ret != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err code=%08x, %s:%d\n", s32Ret, __FILE__, __LINE__);
		exit(-1);
	}
}

// 图像阈值化(源数据只能为单分量) 注：这里的类型固定了...
void threshold(const hiMat &src, hiMat &dst, unsigned int threshold, 
	           unsigned int max_value) // , IVE_THRESH_OUT_FMT_E type)
{
	int s32Ret;

	dst.create(src.rows, src.cols, src.type);// hiMat 负责处理失败情况 ...
	IVE_THRESH_CTRL_S pstThreshCtrl;
	pstThreshCtrl.enOutFmt = IVE_THRESH_OUT_FMT_BINARY;
	pstThreshCtrl.u32MaxVal = max_value; // 255;
	pstThreshCtrl.u32MinVal = 0;
	pstThreshCtrl.u32Thresh = threshold; // 55/22;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src.flush();

	s32Ret = HI_MPI_IVE_THRESH(&IveHandle, &src_info, &dst_mem_info, &pstThreshCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d..%x\n", __FILE__, __LINE__,s32Ret);
		exit(-1);
	}
}

// 两图像相减(源数据只能为单分量) ...
void absdiff(const hiMat &src1, const hiMat &src2, hiMat &dst)
{
	int s32Ret;

	dst.create(src1.rows, src1.cols, src1.type); // hiMat 负责处理失败情况 ...

	IVE_SUB_OUT_FMT_E enOutFmt;
	enOutFmt = IVE_SUB_OUT_FMT_ABS;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info1 = get_src_info_s(src1);
	IVE_SRC_INFO_S src_info2 = get_src_info_s(src2);

	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src1.flush(), src2.flush();

	s32Ret = HI_MPI_IVE_SUB(&IveHandle, &src_info1, &src_info2, &dst_mem_info, enOutFmt, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
}

// 图像或运算(源数据只能为单分量) ...
void bit_or(const hiMat &src1, const hiMat &src2, hiMat &dst)
{
	int s32Ret;

	dst.create(src1.rows, src1.cols, src1.type); // hiMat 负责处理失败情况 ...

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info1 = get_src_info_s(src1);
	IVE_SRC_INFO_S src_info2 = get_src_info_s(src2);

	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src1.flush(), src2.flush();

	s32Ret = HI_MPI_IVE_OR(&IveHandle, &src_info1, &src_info2, &dst_mem_info, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
}

// yuv转rgb(源数据分量可以是sp420, sp422) ...
// 宽度必须是偶数 ...
void yuv2rgb(const hiMat &src, hiMat &dst)
{
	int s32Ret;


	dst.create(src.rows, src.cols, hiMat::RGB24); // hiMat 负责处理失败情况 ...
	IVE_CSC_CTRL_S pstCscCtrl;
	pstCscCtrl.enOutFmt = IVE_CSC_OUT_FMT_PACKAGE;
	pstCscCtrl.enCscMode = IVE_CSC_MODE_VIDEO_BT601_AND_BT656;

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);
	dst_mem_info.u32Stride = dst.cols;

	src.flush();

	s32Ret = HI_MPI_IVE_CSC(&IveHandle, &src_info, &dst_mem_info, &pstCscCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_CSC err code=%08x, %s:%d\n", s32Ret, __FILE__, __LINE__);
		exit(-1);
	}
}

// 积分图(源数据只能为单分量) ...
// 输出数据内存大小必须为：源图像高 * 输出数据跨度 * 8 ...
void integral(const hiMat &src, hiMat &dst)
{
	int s32Ret;

	dst.create(src.rows, src.cols, hiMat::U64); // hiMat 负责处理失败情况 ...

	HI_BOOL bInstant = HI_TRUE;
	IVE_HANDLE IveHandle;

	IVE_SRC_INFO_S src_info = get_src_info_s(src);
	IVE_MEM_INFO_S dst_mem_info = get_mem_info_s(dst);

	src.flush();

	s32Ret = HI_MPI_IVE_INTEG(&IveHandle, &src_info, &dst_mem_info, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		fprintf(stderr, "FATAL: HI_MPI_IVE_DILATE err %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
}

} // namespace hi

#endif // DEBUG_HIMAT
