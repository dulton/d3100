#include "hi_mat.h"
#include "hi_comm_ive.h"
#include "mpi_ive.h"

/** 应该编写 hlp_xxx 用了alloc, copy, free ...

 */

hiMat::hiMat()
	: ref_(1)
	, using_ref_(true)
{

}

hiMat::hiMat(const cv::Mat &m)
{
}

hiMat::~hiMat()
{
	ref_--;
	if (ref_ == 0) {
		// TODO: free mem
	}
}

void hiMat::download(cv::Mat &m)
{
}

void hiMat::create(int rows, int cols, int type)
{
}

hiMat &hiMat::operator=(const hiMat &m)
{
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

