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

