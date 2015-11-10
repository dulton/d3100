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
#include "../../include/mpi_vi.h"

static int zk_mpi_init()
{
    VB_CONF_S stVbConf;
	int hdcnt = 0;		// 不需要hd
	int sdcnt = 64;
	int hdsize = 1920*1080*2;	// sp420
    int sdsize = 640*480*2;

	memset(&stVbConf, 0, sizeof(stVbConf));

    stVbConf.u32MaxPoolCnt = 256;

    /*ddr0 video buffer*/
    stVbConf.astCommPool[0].u32BlkSize = hdsize;
    stVbConf.astCommPool[0].u32BlkCnt = 8;
    stVbConf.astCommPool[0].acMmzName[0] = 0;

    /*ddr1 video buffer*/
    stVbConf.astCommPool[1].u32BlkSize = hdsize;
    stVbConf.astCommPool[1].u32BlkCnt = 8;
    strcpy(stVbConf.astCommPool[1].acMmzName,"ddr1");

	// for sub channel ...

    /*ddr0 video buffer*/
    stVbConf.astCommPool[2].u32BlkSize = sdsize;
    stVbConf.astCommPool[2].u32BlkCnt = 8;
    stVbConf.astCommPool[2].acMmzName[0] = 0;

    /*ddr1 video buffer*/
    stVbConf.astCommPool[3].u32BlkSize = sdsize;
    stVbConf.astCommPool[3].u32BlkCnt = 8;
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
	/** 使用 subchannel，可以利用其“拉伸”功能
		 */

	// set mem conf
	int rc = SAMPLE_COMM_VI_MemConfig(SAMPLE_VI_MODE_4_1080P);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "SAMPLE_COMM_VI_MemConfig err, code=%08x\n", rc);
		return -1;
	}

	// start ...
	int dev_result[8], ch_result[16], subch_result[16];
	rc = SAMPLE_COMM_VI_Start2(SAMPLE_VI_MODE_4_1080P, VIDEO_ENCODING_MODE_PAL,
			dev_result, ch_result, subch_result);
	if (rc != HI_SUCCESS) {
		fprintf(stderr, "SAMPLE_COMM_VI_Start err, code=%08x\n", rc);
		return -1;
	}
	
	// 不需要读 main channels 
//	rc = HI_MPI_VI_SetFrameDepth(0, 2);
//	rc = HI_MPI_VI_SetFrameDepth(4, 2);
//	rc = HI_MPI_VI_SetFrameDepth(8, 2);
//	rc = HI_MPI_VI_SetFrameDepth(12, 2);

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

static void save_nv12(int ch, int cnt, VIDEO_FRAME_S *frame)
{
	// sp420 相当于 ffmpeg 中的 nv21，使用命令:
	//   ffmpeg -f rawvideo -s 480x270 -pix_fmt nv21 -i xxxx.yuv output.jpg
	// 转化为 jpg 文件，方便查看 ..
	//
	// 注意：VIDEO_FRAME_S 中的 pVirAddr 是忽悠人的，不能使用，需要自己 mmap

	char fname[128];
	snprintf(fname, sizeof(fname), "saved/y_%i_%05d.yuv", ch, cnt);

	// mmap
	unsigned char *y = HI_MPI_SYS_Mmap(frame->u32PhyAddr[0], 
			frame->u32Stride[0] * frame->u32Height);
	printf("#### width = %d, ####height = %d\n", frame->u32Width, frame->u32Height);
	unsigned char *uv = HI_MPI_SYS_Mmap(frame->u32PhyAddr[1],
			frame->u32Stride[1] * frame->u32Height / 2);	// uv 交错保存

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

static double now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

static int zk_lock_pic(int ch, VIDEO_FRAME_INFO_S *f)
{
	double t1 = now();
	int rc = HI_MPI_VI_GetFrame(ch, f);
	double t2 = now();
	fprintf(stderr, "[%d] GetFrame using %.3f\n", ch, t2-t1);
	if (rc == HI_SUCCESS) {
		static unsigned int cnt_ = 0;
		save_nv12(ch, cnt_++, &f->stVFrame);
		return 0;
	}
	else {
		fprintf(stderr, "[%d] GetFrame err %08x\n", ch, rc);
		return -1;
	}
}

static void zk_unlock_pic(int ch, VIDEO_FRAME_INFO_S *f)
{
	HI_MPI_VI_ReleaseFrame(ch, f);
}

static int zk_run()
{
	// 每隔几秒查询vi，确认有效输入
	int i;
	VI_CHN_STAT_S stats[16];	// 照理说还需要增加subchn的 ...
	int ch_valids[16];			// 通过多次 query，保存当前可用的 channel ...
	unsigned int ch_last_intcnt[16];	// 记录上次 intr cnt 

	for (i = 0; i < 16; i++) {
		ch_valids[i] = 0;
		ch_last_intcnt[i] = 0;
	}

	usleep(1000*1000);	// 等 ..

	while (1) {
		for (i = 0; i < 16; i++) {
			int rc = HI_MPI_VI_Query(i, &stats[i]);
			if (rc == HI_SUCCESS) {
				VI_CHN_STAT_S *s = &stats[i];
				if (s->bEnable) {
					if (s->u32IntCnt == ch_last_intcnt[i]) {
						// 没有变化，说明通道没有中断 ...
						ch_valids[i] = 0;
					}
					else {
						ch_valids[i] = 1;
						ch_last_intcnt[i] = s->u32IntCnt;
					}
				}
			}
		}

		// 100毫秒，提取一帧图像 ...
		usleep(100*1000);

		fprintf(stderr, "actived:");
		for (i = 0; i < 16; i++) {
			if (ch_valids[i]) {
				fprintf(stderr, " [%d]", i);
			}
		}
		fprintf(stderr, "\n");

		// 对 sub channel 进行读取 ...
		for (i = 0; i < 16; i++) {
			if (ch_valids[i]) {
			//if (i == 0) {
				VIDEO_FRAME_INFO_S frame;
				int rc = zk_lock_pic(SUBCHN(i), &frame);
				if (rc == 0) {
					zk_unlock_pic(SUBCHN(i), &frame);
				}
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int rc = zk_mpi_init();
	if (rc != 0) {
		return rc;
	}

	fprintf(stdout, "mpi_init ok!\n");

	rc = zk_vi_init();
	if (rc != 0) {
		return rc;
	}

	rc = zk_run();

	rc = zk_vi_uninit();
	rc = zk_mpi_uninit();

	return 0;
}

