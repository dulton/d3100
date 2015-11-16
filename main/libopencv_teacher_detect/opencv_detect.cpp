#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "sample_comm.h"
#include <errno.h>
#include "mpi_vi.h"

#include  "hi_comm_ive.h"
#include  "hi_type.h"
#include  "mpi_ive.h"
#include "mpi_sys.h"
#include "hi_common.h"
#include"hi_comm_sys.h"
#include "hi_comm_video.h"
#include "mpi_vb.h"

#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;

#include "detect.h"

#include "sys/timeb.h"

//问题1： string str返回是否正确 ...
//问题2：VIDEO_FRAME_INFO_S frame 是否可直接赋值 ...


struct hi_detect_t
{
	bool is_quit;
	det_t *det;
	string str;
    int ch; // 通道 ...
	VIDEO_FRAME_INFO_S frame;
	pthread_t thread_id;
	pthread_mutex_t mutex;
};

static int zk_mpi_init()
{
    VB_CONF_S stVbConf;
	int hdcnt = 0;		// 不需要hd
	int sdcnt = 64;
	int hdsize = 1920*1080*2;	// sp420
    int sdsize = 480*270*2;

	memset(&stVbConf, 0, sizeof(stVbConf));

    stVbConf.u32MaxPoolCnt = 256;

    /*ddr0 video buffer*/
    stVbConf.astCommPool[0].u32BlkSize = hdsize;
    stVbConf.astCommPool[0].u32BlkCnt = hdcnt;
    stVbConf.astCommPool[0].acMmzName[0] = 0;

    /*ddr1 video buffer*/
    stVbConf.astCommPool[1].u32BlkSize = hdsize;
    stVbConf.astCommPool[1].u32BlkCnt = hdcnt;
    strcpy(stVbConf.astCommPool[1].acMmzName,"ddr1");

	// for sub channel ...

    /*ddr0 video buffer*/
    stVbConf.astCommPool[2].u32BlkSize = sdsize;
    stVbConf.astCommPool[2].u32BlkCnt = sdcnt;
    stVbConf.astCommPool[2].acMmzName[0] = 0;

    /*ddr1 video buffer*/
    stVbConf.astCommPool[3].u32BlkSize = sdsize;
    stVbConf.astCommPool[3].u32BlkCnt = sdcnt;
    strcpy(stVbConf.astCommPool[3].acMmzName,"ddr1");

    int rc = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != rc)
    {
        SAMPLE_PRT("system init failed with %d!\n", rc);
		return -1;
    }

	return 0;
}

static int zk_mpi_uninit()
{
	HI_S32 rc = HI_MPI_SYS_Exit();
	rc = HI_MPI_VB_Exit();
	return 0;
}

static int zk_vi_init()
{
	/** 使用 subchannel，可以利用其“拉伸”功能 */

	// set mem conf
	int rc = SAMPLE_COMM_VI_MemConfig(SAMPLE_VI_MODE_4_1080P);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "SAMPLE_COMM_VI_MemConfig err, code=%08x\n", rc);
		return -1;
	}

	// start ...
	int dev_result[8], ch_result[16], subch_result[16];
	rc = SAMPLE_COMM_VI_Start(SAMPLE_VI_MODE_4_1080P, VIDEO_ENCODING_MODE_PAL);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "SAMPLE_COMM_VI_Start err, code=%08x\n", rc);
		return -1;
	}
	
	// 不需要读 main channels 
    //rc = HI_MPI_VI_SetFrameDepth(0, 2);
    //rc = HI_MPI_VI_SetFrameDepth(4, 2);
    //rc = HI_MPI_VI_SetFrameDepth(8, 2);
    //rc = HI_MPI_VI_SetFrameDepth(12, 2);

	// 为了读 sub channels，否则，会出现 0xa010800e 错误
	rc = HI_MPI_VI_SetFrameDepth(16, 4);
	//rc = HI_MPI_VI_SetFrameDepth(20, 4);
	//rc = HI_MPI_VI_SetFrameDepth(24, 4);
	rc = HI_MPI_VI_SetFrameDepth(28, 4);

	return 0;

}

static int zk_vi_uninit()
{
	return 0;
}

void hi_init(hi_detect_t* hi_det, const char *kvfname)
{
	hi_det->is_quit = false;

	hi_det->ch = 16; // 设置输入主通道 ...

	int rc = zk_mpi_init();
	if (rc != 0) {
		 fprintf(stderr, "hi_mpi initialization failed!\n");
	}
   
	rc = zk_vi_init();
	if (rc != 0) {
		fprintf(stderr, "hi_vi initialization failed!\n");
	}

	hi_det->det = det_open(kvfname);

}

void hi_uninit(hi_detect_t *hi_det)
{
	hi_det->is_quit = true;	

	int rc = zk_mpi_uninit();

	rc = zk_vi_uninit();

	det_close(hi_det->det);

	HI_MPI_VI_ReleaseFrame(hi_det->ch, &hi_det->frame);
}


static void save_nv12(int ch, int cnt, VIDEO_FRAME_S *frame)
{
	// sp420 相当于 ffmpeg 中的 nv21，使用命令:
	// ffmpeg -f rawvideo -s 480x270 -pix_fmt nv21 -i xxxx.yuv output.jpg
	// 转化为 jpg 文件，方便查看 ...
	// 注意：VIDEO_FRAME_S 中的 pVirAddr 是忽悠人的，不能使用，需要自己 mmap

	char fname[128];
	snprintf(fname, sizeof(fname), "saved/y_%i_%05d.yuv", ch, cnt);

	// mmap
	unsigned char *y = (unsigned char *)HI_MPI_SYS_Mmap(frame->u32PhyAddr[0], 
			frame->u32Stride[0] * frame->u32Height);
	unsigned char *uv = (unsigned char *)HI_MPI_SYS_Mmap(frame->u32PhyAddr[1],
			frame->u32Stride[1] * frame->u32Height / 2);	// uv 交错保存 ...

	FILE *fp = fopen(fname, "wb");
	if (fp) {
		// save Y
		fwrite(y, 1, frame->u32Stride[0]*frame->u32Height, fp);

		// save uv
		fwrite(uv, 1, frame->u32Stride[0]*frame->u32Height/2, fp);

		fclose(fp);
	}
	else {
		fprintf(stderr, "---- can't open %s\n", fname);
	}

	// munmap
	HI_MPI_SYS_Munmap(uv, frame->u32Stride[1] * frame->u32Height/2);
	HI_MPI_SYS_Munmap(y, frame->u32Stride[0] * frame->u32Height);
}

static void save_nv1(IVE_SRC_INFO_S *stSrc, HI_VOID *pVirtSrc)
{
	// sp420 相当于 ffmpeg 中的 nv21，使用命令:
	// ffmpeg -f rawvideo -s 480x270 -pix_fmt nv21 -i xxxx.yuv output.jpg
	// 转化为 jpg 文件，方便查看 ..
	// 注意：VIDEO_FRAME_S 中的 pVirAddr 是忽悠人的，不能使用，需要自己 mmap

	char fname[128];
	snprintf(fname, sizeof(fname), "saved1/yuv_image.yuv");

	unsigned char *y = (uchar *)pVirtSrc;
	unsigned char *uv = (uchar *)pVirtSrc + (stSrc->u32Height * stSrc->u32Width);

	FILE *fp = fopen(fname, "wb");
	if (fp) {
		// save YUV
		fwrite(y, 1, stSrc->stSrcMem.u32Stride * stSrc->u32Height, fp);

		fwrite(uv, 1, stSrc->stSrcMem.u32Stride * (stSrc->u32Height / 2), fp);

		fclose(fp);
	}
	else {
		fprintf(stderr, "---- can't open %s\n", fname);
	}

}

// VIDEO_FRAME_INFO_S转化为Mat矩阵;
// 处理一：模糊处理;
// 处理二：YUV->RGB;
static string process_frame(hi_detect_t *hi_det)
{
	timeb pre,cur;
    ftime(&pre);

	VIDEO_FRAME_INFO_S *stFrame = &hi_det->frame; // ????...

	HI_S32 s32Ret;
	IVE_SRC_INFO_S stSrc;
	IVE_MEM_INFO_S stDst;
	IVE_HANDLE IveHandle;
	HI_BOOL bInstant;
	HI_VOID *pVirtSrc;
	HI_VOID *pVirtDst;

	// *********** stFrame中的数据转存到stSrc中 ***********;
	stSrc.u32Width = stFrame->stVFrame.u32Width;
	stSrc.u32Height = stFrame->stVFrame.u32Height;
	stSrc.enSrcFmt = IVE_SRC_FMT_SP420;
	//stSrc.stSrcMem.u32PhyAddr = stFrame->stVFrame.u32PhyAddr[0];//????;
	stSrc.stSrcMem.u32Stride = stFrame->stVFrame.u32Stride[0];
	bInstant = HI_TRUE;

	s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stSrc.stSrcMem.u32PhyAddr, &pVirtSrc,
		     "User", HI_NULL, (3 * stSrc.u32Height / 2.0) * stSrc.stSrcMem.u32Stride);
	if(s32Ret != HI_SUCCESS)
	{
		printf("can't alloc intergal memory for %x\n",s32Ret);
	    return HI_NULL;
	}
	
	uchar * p = (uchar *)HI_MPI_SYS_Mmap(stFrame->stVFrame.u32PhyAddr[0], 
		         stFrame->stVFrame.u32Stride[0] * (3 * stFrame->stVFrame.u32Height / 2.0));

	memcpy((uchar *)pVirtSrc, p, stSrc.stSrcMem.u32Stride * (3 * stSrc.u32Height / 2.0));

	// *********** 图像滤波 ***********;
	s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stDst.u32PhyAddr, &pVirtDst, 
			 "User", HI_NULL, (3 * stSrc.u32Height / 2) * stSrc.stSrcMem.u32Stride);
	if(s32Ret != HI_SUCCESS)
	{
		printf("can't alloc intergal memory for %x\n", s32Ret);
		return HI_NULL;
	}			
	stDst.u32Stride = stFrame->stVFrame.u32Stride[0];

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
	s32Ret = HI_MPI_IVE_FILTER(&IveHandle, &stSrc, &stDst, &pstFilterCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		printf(" ive filter function can't submmit for %x\n", s32Ret);
		return HI_NULL;
	}
	else
	{
		printf("sucess filter\n");
		//save_nv1(&stSrc, pVirtDst);
	}

	// *********** YUV转到RGB空间 ***********;
	// stSrc_rgb指向滤波后数据 ...
	IVE_SRC_INFO_S stSrc_rgb;
	memcpy(&stSrc_rgb, &stSrc, sizeof(IVE_SRC_INFO_S));
	stSrc_rgb.stSrcMem.u32PhyAddr = stDst.u32PhyAddr;

	IVE_MEM_INFO_S stDst_rgb;
	HI_VOID *pVirtDst_rgb;

	s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stDst_rgb.u32PhyAddr, &pVirtDst_rgb,
			"User", HI_NULL, stSrc.u32Height * stSrc.stSrcMem.u32Stride * 3);
	stDst_rgb.u32Stride = stSrc.stSrcMem.u32Stride;
    if(s32Ret != HI_SUCCESS)
	{
		printf("can't alloc intergal memory for %x\n", s32Ret);
		HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pVirtDst);
	    return HI_NULL;
	}

	IVE_CSC_CTRL_S pstCscCtrl;
	pstCscCtrl.enOutFmt = IVE_CSC_OUT_FMT_PACKAGE;
	pstCscCtrl.enCscMode = IVE_CSC_MODE_VIDEO_BT601_AND_BT656;

	s32Ret = HI_MPI_IVE_CSC(&IveHandle, &stSrc_rgb, &stDst_rgb, &pstCscCtrl, bInstant);
	if(s32Ret != HI_SUCCESS)
	{
		printf(" ive CSC function can't submmit for %x\n", s32Ret);
		return HI_NULL;
	}
	else
	{
		printf("sucess csc\n");
	}

	// *********** 创建RGB目标矩阵 ***********;
	cv::Mat image;
	image.create(stSrc.u32Height, stSrc.u32Width, CV_8UC3);
	int buflen = stSrc.u32Height * stSrc.u32Width * 3;
	memcpy(image.data, (uchar *)pVirtDst_rgb, buflen);

	// *********** 目标探测算法 ***********;
	const char* s;
	s = det_detect(hi_det->det, image);    ftime(&cur);
    double time = double((cur.time - pre.time) * 1000 + (cur.millitm - pre.millitm));
	printf("&&&&&&&&&&detect time: %f\n", time);
	
	HI_MPI_SYS_MmzFree(stDst_rgb.u32PhyAddr, pVirtDst_rgb);
    HI_MPI_SYS_MmzFree(stSrc.stSrcMem.u32PhyAddr, pVirtSrc);
	//HI_MPI_SYS_MmzFree(stSrc_rgb.stSrcMem.u32PhyAddr, pVirtDst);
	HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pVirtDst);	

	return s;
}


void* thread_proc(void *pdet)
{	
	hi_detect_t *hi_det = (hi_detect_t*)pdet;

	int rc = HI_SUCCESS;

	while(!hi_det->is_quit){
		VIDEO_FRAME_INFO_S frame;

		rc = HI_MPI_VI_GetFrame(SUBCHN(hi_det->ch), &frame);
		if(rc != HI_SUCCESS){
			usleep(10 * 1000);
			continue;
		}

		pthread_mutex_lock(&hi_det->mutex);
		hi_det->frame = frame; // 结构体变量可相互赋值???????.....
		pthread_mutex_unlock(&hi_det->mutex);

		HI_MPI_VI_ReleaseFrame(hi_det->ch, &frame);

		usleep(200 * 1000); // 每200ms探测一次 ...

	}
}

// 初始化失败没有考虑呢 ??? ...
hi_detect_t* hi_det_open(const char *kvfname)
{
	hi_detect_t *hi_det = new hi_detect_t;

	hi_init(hi_det, kvfname);

	int rc = pthread_mutex_init(&hi_det->mutex, NULL);

	if (rc != 0){
		fprintf(stderr, "Mutex initialization failed!");
	}

	pthread_create(&hi_det->thread_id, 0, thread_proc, (void*)hi_det);

	return hi_det;
}

const char* hi_det_detect(hi_detect_t *hi_det)
{
	pthread_mutex_lock(&hi_det->mutex);
	hi_det->str = process_frame(hi_det);
	pthread_mutex_unlock(&hi_det->mutex);

	return hi_det->str.c_str();	
}

void hi_det_colse(hi_detect_t *hi_det)
{
	hi_uninit(hi_det);

	pthread_join(hi_det->thread_id, 0);

	pthread_mutex_destroy(&hi_det->mutex);

	delete hi_det;
}





