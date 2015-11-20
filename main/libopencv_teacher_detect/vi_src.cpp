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
#include <errno.h>
#include "mpi_vi.h"

#include "hi_comm_ive.h"
#include "hi_type.h"
#include "mpi_ive.h"
#include "mpi_sys.h"
#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_video.h"
#include "mpi_vb.h"
#include "hi_mat.h"

#include "vi_src.h"
using namespace cv;
using namespace std;
#include "KVConfig.h"

#include "sys/timeb.h"

#define FATAL(func, file, line) { fprintf(stderr, "FATAL: exit %s, %s:%d\n", func, file, line);  exit(-1);  }

/** TODO: 去掉所有 SAMPLE_XXX 的调用,恢复 MPI 本来面目 ....
 */

static int zk_mpi_init()
{
	VB_CONF_S stVbConf;
	int hdcnt = 0;		// 不需要hd
	int sdcnt = 64;
	int hdsize = 1920 * 1080 * 2;	// sp420
	int sdsize = 480 * 270 * 2;

	memset(&stVbConf, 0, sizeof(stVbConf));

	stVbConf.u32MaxPoolCnt = 256;

	/*ddr0 video buffer */
	stVbConf.astCommPool[0].u32BlkSize = hdsize;
	stVbConf.astCommPool[0].u32BlkCnt = hdcnt;
	stVbConf.astCommPool[0].acMmzName[0] = 0;

	/*ddr1 video buffer */
	stVbConf.astCommPool[1].u32BlkSize = hdsize;
	stVbConf.astCommPool[1].u32BlkCnt = hdcnt;
	strcpy(stVbConf.astCommPool[1].acMmzName, "ddr1");

	// for sub channel ...

	/*ddr0 video buffer */
	stVbConf.astCommPool[2].u32BlkSize = sdsize;
	stVbConf.astCommPool[2].u32BlkCnt = sdcnt;
	stVbConf.astCommPool[2].acMmzName[0] = 0;

	/*ddr1 video buffer */
	stVbConf.astCommPool[3].u32BlkSize = sdsize;
	stVbConf.astCommPool[3].u32BlkCnt = sdcnt;
	strcpy(stVbConf.astCommPool[3].acMmzName, "ddr1");

	HI_MPI_SYS_Exit();
	HI_MPI_VB_Exit();

	int rc = HI_MPI_VB_SetConf(&stVbConf);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	rc = HI_MPI_VB_Init();
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	MPP_SYS_CONF_S sc;
	sc.u32AlignWidth = 16;
	rc = HI_MPI_SYS_SetConf(&sc);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	rc = HI_MPI_SYS_Init();
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int zk_mpi_uninit()
{
	HI_S32 rc = HI_MPI_SYS_Exit();
	rc = HI_MPI_VB_Exit();
	return 0;
}

static void vi_setmask(int ViDev, VI_DEV_ATTR_S * pstDevAttr)
{
	switch (ViDev % 4) {
	case 0:
		pstDevAttr->au32CompMask[0] = 0xFF000000;
		if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0x00FF0000;
		else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0x0;
		break;
	case 1:
		pstDevAttr->au32CompMask[0] = 0xFF0000;
		if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0x0;
		break;
	case 2:
		pstDevAttr->au32CompMask[0] = 0xFF00;
		if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0xFF;
		else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0x0;
		break;
	case 3:
		pstDevAttr->au32CompMask[0] = 0xFF;
		if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
			pstDevAttr->au32CompMask[1] = 0x0;
		break;
	default:
		HI_ASSERT(0);
	}
}

static VI_DEV_ATTR_S DEV_ATTR_7441_BT1120_1080P = {
	VI_MODE_BT1120_STANDARD,
	VI_WORK_MODE_1Multiplex,
	{
		0xFF000000, 0xFF0000
	},
	VI_SCAN_PROGRESSIVE,
	{
		-1, -1, -1, -1
	},
	VI_INPUT_DATA_UVUV,
	{
		VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH,
	 	VI_HSYNC_VALID_SINGNAL, VI_HSYNC_NEG_HIGH,
	 	VI_VSYNC_NORM_PULSE, VI_VSYNC_VALID_NEG_HIGH,
	 	{
			0, 1920, 0,
	  		0, 1080, 0,
	  		0, 0, 0
		}
	}
};

static int vi_start_dev(int devid)
{
	VI_DEV_ATTR_S vdas;
	memset(&vdas, 0, sizeof(vdas));

	memcpy(&vdas, &DEV_ATTR_7441_BT1120_1080P, sizeof(vdas));
	vi_setmask(devid, &vdas);

	int rc = HI_MPI_VI_SetDevAttr(devid, &vdas);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	rc = HI_MPI_VI_EnableDev(devid);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int vi_stop_dev(int dev)
{
	int rc = HI_MPI_VI_DisableDev(dev);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int vi_start_ch(int ch, int width, int height)
{
	VI_CHN_ATTR_S attr;
	attr.stCapRect.s32X = 0;	// FIXME: 应该合理设置cap rect
	attr.stCapRect.s32Y = 0;
	attr.stCapRect.u32Width = 1920;
	attr.stCapRect.u32Height = 1080;
	attr.stDestSize.u32Width = width;
	attr.stDestSize.u32Height = height;
	attr.enCapSel = VI_CAPSEL_BOTH;
	attr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	attr.bMirror = HI_FALSE;
	attr.bFlip = HI_FALSE;
	attr.bChromaResample = HI_FALSE;
	attr.s32SrcFrameRate = -1;
	attr.s32FrameRate = -1;

	int rc = HI_MPI_VI_SetChnAttr(ch, &attr);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	rc = HI_MPI_VI_EnableChn(ch);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int vi_stop_ch(int ch)
{
	int rc = HI_MPI_VI_DisableChn(ch);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int sys_mem_conf(int ch)
{
	MPP_CHN_S mcs;
	mcs.enModId = HI_ID_VIU;
	mcs.s32DevId = 0;	// VIU 必须使用 0
	mcs.s32ChnId = ch;

	int rc = HI_MPI_SYS_SetMemConf(&mcs, 0);	// ddr0
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

/** 初始化探测使用的通道 */
static int zk_vi_init(int ch, int vw, int vh)
{
	/** 使用 subchannel，可以利用其“拉伸”功能 */
	int rc;
	int devid = ch / 2;	// XXX:   devid = { 0, 2, 4, 6}
	//                      chid = { 0, 4, 8, 12 }
	//                      subchid = { 16, 20, 24, 28 }
	int subch = ch + 16;

	// mem conf
	sys_mem_conf(ch);
	sys_mem_conf(subch);

	// start dev
	vi_start_dev(devid);

	// start channel
	vi_start_ch(ch, 1920, 1080); // FIXME: 如果不启动main ch，sub ch 也无法启动 ..
	vi_start_ch(subch, vw, vh); 

	// ???
	rc = HI_MPI_VI_SetFrameDepth(subch, 4);
	if (rc != HI_SUCCESS) {
		FATAL(__func__, __FILE__, __LINE__);
	}

	return 0;
}

static int zk_vi_uninit(int ch)
{
	int subch = ch + 16, dev = ch / 2;

	vi_stop_ch(ch);
	vi_stop_ch(subch);
	vi_stop_dev(dev);

	return 0;
}

static void save_nv12(int ch, int cnt, VIDEO_FRAME_S * frame)
{
	// sp420 相当于 ffmpeg 中的 nv21，使用命令:
	// ffmpeg -f rawvideo -s 480x270 -pix_fmt nv21 -i xxxx.yuv output.jpg
	// 转化为 jpg 文件，方便查看 ...
	// 注意：VIDEO_FRAME_S 中的 pVirAddr 是忽悠人的，不能使用，需要自己 mmap

	char fname[128];
	snprintf(fname, sizeof(fname), "saved/y_%i_%05d.yuv", ch, cnt);

	// mmap
	unsigned char *y =
	    (unsigned char *)HI_MPI_SYS_Mmap(frame->u32PhyAddr[0],
					     frame->u32Stride[0] *
					     frame->u32Height);
	unsigned char *uv =
	    (unsigned char *)HI_MPI_SYS_Mmap(frame->u32PhyAddr[1],
					     frame->u32Stride[1] * frame->u32Height / 2);	// uv 交错保存 ...

	FILE *fp = fopen(fname, "wb");
	if (fp) {
		// save Y
		fwrite(y, 1, frame->u32Stride[0] * frame->u32Height, fp);

		// save uv
		fwrite(uv, 1, frame->u32Stride[0] * frame->u32Height / 2, fp);

		fclose(fp);
	} else {
		fprintf(stderr, "---- can't open %s\n", fname);
	}

	// munmap
	HI_MPI_SYS_Munmap(uv, frame->u32Stride[1] * frame->u32Height / 2);
	HI_MPI_SYS_Munmap(y, frame->u32Stride[0] * frame->u32Height);
}

static void save_nv1(const char *fname, IVE_SRC_INFO_S * stSrc,
		     HI_VOID * pVirtSrc)
{
	// sp420 相当于 ffmpeg 中的 nv21，使用命令:
	// ffmpeg -f rawvideo -s 480x270 -pix_fmt nv21 -i xxxx.yuv output.jpg
	// 转化为 jpg 文件，方便查看 ..
	// 注意：VIDEO_FRAME_S 中的 pVirAddr 是忽悠人的，不能使用，需要自己 mmap

	unsigned char *y = (uchar *) pVirtSrc;
	unsigned char *uv =
	    (uchar *) pVirtSrc + (stSrc->u32Height * stSrc->stSrcMem.u32Stride);

	FILE *fp = fopen(fname, "wb");
	if (fp) {
		// save YUV
		fwrite(y, 1, stSrc->stSrcMem.u32Stride * stSrc->u32Height, fp);
		fwrite(uv, 1,
		       stSrc->stSrcMem.u32Stride * (stSrc->u32Height / 2), fp);
		fclose(fp);
	} else {
		fprintf(stderr, "---- can't open %s\n", fname);
	}
}

static void save_rgb(int stride, int width, int height, void *data)
{
	const char *filename = "saved/test_image.rgb";

	FILE *fp = fopen(filename, "wb");
	if (fp) {
		fwrite(data, 1, width * height * 3, fp);
#if 0
		uchar *p = (uchar *) data;
		fprintf(stdout, "$$$$$$$$p length %d\n", strlen(p));
		for (int i = 0; i < height; i++) {
			fwrite(p, width * 3, 1, fp);
			p += stride;
		}
#endif
		fclose(fp);
	}
}

static void save_mat(const cv::Mat & m, const char *fname)
{
	fprintf(stderr, "DEBUG: m: (%d,%d)\n", m.cols, m.rows);
	FILE *fp = fopen(fname, "wb");
	if (fp) {
		for (int y = 0; y < m.rows; y++) {
			const unsigned char *p = m.ptr < unsigned char >(y);
			fwrite(p, 1, m.elemSize() * m.cols, fp);
		}

		fclose(fp);
	}
}

#if 0
/** 将 frame 转换为 rgb24 的 Mat */
static void vf2mat(VIDEO_FRAME_INFO_S & frame, cv::Mat & m)
{
	IVE_HANDLE IveHandle;
	HI_BOOL bInstant;

	int width = frame.stVFrame.u32Width, height = frame.stVFrame.u32Height;

	size_t yuvsize = width * height * 3 / 2;	// yuv nv12
	size_t rgbsize = width * height * 3;	// rgb24

	IVE_SRC_INFO_S src;
	void *src_vir;
	src.u32Width = width;
	src.u32Height = height;
	src.enSrcFmt = IVE_SRC_FMT_SP420;	// nv12
	src.stSrcMem.u32Stride = frame.stVFrame.u32Stride[0];

	// 准备 ive src 内存 ..
	int rc = HI_MPI_SYS_MmzAlloc_Cached(&src.stSrcMem.u32PhyAddr, &src_vir,
					    "User", 0, yuvsize);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}
	// 将 frame 中的数据复制到 ive src 中 ...
	uchar *p =
	    (uchar *) HI_MPI_SYS_Mmap(frame.stVFrame.u32PhyAddr[0], yuvsize);
	memcpy(src_vir, p, yuvsize);
	HI_MPI_SYS_Munmap(p, yuvsize);

    //save_nv1("saved/src000.yuv", &src, src_vir);

	// 准备 ive dst 内存 ..
	IVE_MEM_INFO_S dst;
	dst.u32Stride = frame.stVFrame.u32Stride[0];
	void *dst_vir;
	rc = HI_MPI_SYS_MmzAlloc_Cached(&dst.u32PhyAddr, &dst_vir, "User", 0,
					yuvsize);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}
	// ive 滤波 ...
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

	// XXX: FlushCache 保证 src_vir 中是可靠的数据 ...
	HI_MPI_SYS_MmzFlushCache(src.stSrcMem.u32PhyAddr, src_vir, yuvsize);
	rc = HI_MPI_IVE_FILTER(&IveHandle, &src, &dst, &pstFilterCtrl,
			       bInstant);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_IVE_FILTER err %s:%d\n",
			__FILE__, __LINE__);
		exit(-1);
	}
	// src 内存不再需要 ..
	HI_MPI_SYS_MmzFree(src.stSrcMem.u32PhyAddr, src_vir);

	// yuv to rgb
	IVE_MEM_INFO_S rgb;
	void *rgb_vir;
	rc = HI_MPI_SYS_MmzAlloc_Cached(&rgb.u32PhyAddr, &rgb_vir, "User", 0,
					rgbsize);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_SYS_MmzAlloc_Cached err %s:%s\n",
			__FILE__, __LINE__);
		exit(-1);
	}
	rgb.u32Stride = width;	// XXX: IVE 中的 stride 与众不同 ..

	IVE_SRC_INFO_S yuv;	// 直接使用滤波后的 dst
	yuv.u32Width = width;
	yuv.u32Height = height;
	yuv.stSrcMem = dst;
	yuv.enSrcFmt = IVE_SRC_FMT_SP420;

	IVE_CSC_CTRL_S pstCscCtrl;
	pstCscCtrl.enOutFmt = IVE_CSC_OUT_FMT_PACKAGE;
	pstCscCtrl.enCscMode = IVE_CSC_MODE_VIDEO_BT601_AND_BT656;

	//save_nv1("saved/filtered.yuv", &yuv, dst_vir);

	//HI_MPI_SYS_MmzFlushCache(dst.u32PhyAddr, dst_vir, yuvsize);
	rc = HI_MPI_IVE_CSC(&IveHandle, &yuv, &rgb, &pstCscCtrl, bInstant);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "FATAL: HI_MPI_IVE_CSC err %s:%d\n", __FILE__,
			__LINE__);
		fprintf(stderr, "ERRO NO = 0x%x\n", rc);
		exit(-1);
	}
	// dst 内存不再需要 ..
	HI_MPI_SYS_MmzFree(dst.u32PhyAddr, dst_vir);

	// 将rgb数据复制到 mat 中 ...
	m.create(height, width, CV_8UC3);

	//save_rgb(0, width, height, rgb_vir);

	char *s = (char *)rgb_vir;
	for (int i = 0; i < height; i++) {
		unsigned char *d = m.ptr < unsigned char >(i);
		memcpy(d, s, m.elemSize() * m.cols);
		s += width * 3;	// FIXME:
	}

	//save_mat(m, "saved/mat.rgb");

	HI_MPI_SYS_MmzFree(rgb.u32PhyAddr, rgb_vir);
}
#else
static void vf2mat(VIDEO_FRAME_INFO_S &v, cv::Mat &m)
{
	hiMat hm(v.stVFrame.u32PhyAddr[0], v.stVFrame.u32Width, 
			v.stVFrame.u32Height, v.stVFrame.u32Stride[0], hiMat::SP420);
	hm.dump_hdr();

	hiMat t1, t2;
//	hi::filter(hm, t1);
//	t1.dump_hdr();

	hi::yuv2rgb(hm, t2);
	t2.dump_hdr();
	t2.download(m);

	save_mat(m, "saved/mat0.rgb");
	fprintf(stderr, "DEBUG: %s, %d\n", __func__, __LINE__);
	exit(-1);
}
#endif // 

struct visrc_t {
	KVConfig *cfg;
	int ch;			// 用于vi的通道，默认 28
	int vwidth, vheight;	// video_width, video_height
};

visrc_t *vs_open(const char *fname)
{
	static bool _inited = false;
	if (!_inited) {
		if (zk_mpi_init() < 0) {
			fprintf(stderr, "FATAL: zk_mpi_init err!!!\n");
			exit(-1);
		}
		_inited = true;
	}

	visrc_t *vs = new visrc_t;
	vs->cfg = new KVConfig(fname);
	vs->vwidth = atoi(vs->cfg->get_value("video_width", "480"));
	vs->vheight = atoi(vs->cfg->get_value("video_height", "270"));
	vs->ch = atoi(vs->cfg->get_value("vi_ch", "12"));
	zk_vi_init(vs->ch, vs->vwidth, vs->vheight);	// FIXME:       

	return vs;
}

bool vs_next_frame(visrc_t * vs, cv::Mat & m)
{
	/** FIXME: 这里从 vi 读取一帧图像，如果超时，返回 false 
	 */
	VIDEO_FRAME_INFO_S frame;
	int rc = HI_MPI_VI_GetFrame(SUBCHN(vs->ch), &frame);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "ERR: HI_MPI_VI_GetFrame err, code=%08x\n", rc);
		return false;	// 超时失败 ...
	}
	// 通过 VIDEO_FRAME_INFO_S 构造 cv::Mat
	vf2mat(frame, m);
	HI_MPI_VI_ReleaseFrame(vs->ch, &frame);
	return true;
}

void vs_close(visrc_t * vs)
{
	zk_vi_uninit(vs->ch);		// FIXME:
	delete vs->cfg;
	delete vs;
}
