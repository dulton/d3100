#include <stdio.h>
#include "config.h"
#include "../hi_mat.h"
#include "mpi_sys.h"
#include "mpi_vb.h"

#define DBG(m, line) { fprintf(stderr, "%d: ref=%d\n", line, m.ref()); }

#ifdef DEBUG_HIMAT
#define ASSERT(m, line, n) { \
	if (m.ref() == n) { \
		fprintf(stderr, "%d: PASS\n", line); \
	} \
	else { \
		fprintf(stderr, "%d: ERR: %d, %d\n", line, m.ref(), n); \
	} \
}
#else
#define ASSERT
#endif // 

#define FATAL(func, file, line) { fprintf(stderr, "FATAL: exit %s, %s:%d\n", func, file, line);  exit(-1);  }

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


static void dump_m(const cv::Mat &ret)
{
	for (int i = 0; i < ret.rows; i++) {
		const uchar *p = ret.ptr<uchar>(i);
		for (int j = 0; j < ret.cols; j++) {
			fprintf(stderr, "%02x ", p[j]);
		}

		fprintf(stderr, "\n");
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

// 从 fname 中加载原始rgb数据，保存到 m 中 
static void load_rgb(cv::Mat &m, const char *fname, int w, int h)
{
	FILE *fp = fopen(fname, "rb");
	if (!fp)
		return;

	m.create(h, w, CV_8UC3);
	fread(m.data, 1, w * h * 3, fp);
	fclose(fp);
}


int main()
{
#if 0
	cv::Mat m, m2;
	m = cv::Mat::ones(3, 3, CV_8U);
	dump_m(m);

	hiMat hm;
	hm = m;
	ASSERT(hm, __LINE__, 1);

	hiMat hm2 = hm;

	ASSERT(hm2, __LINE__, 2);
	ASSERT(hm, __LINE__, 2);

	hm2.create(3, 3, 3);
	ASSERT(hm2, __LINE__, 1);

	hm2 = hm2;
	ASSERT(hm2, __LINE__, 1);

	cv::Mat ret;
	hm2.download(ret);
	dump_m(ret);
#else
	zk_mpi_init();

	cv::Mat origin;
	load_rgb(origin, "saved/mat.rgb", 480, 270);

	if (origin.cols != 480) {
		fprintf(stderr, "ERR: load err\n");
		exit(-1);
	}

	fprintf(stderr, "load ok: %d, %d\n", origin.cols, origin.rows);

	cv::Mat gray;
	cv::cvtColor(origin, gray, cv::COLOR_RGB2GRAY);
	fprintf(stderr, "cvtColor to GRAY ok!\n");

	hiMat hm(gray), hm2;

	fprintf(stderr, "before filter!\n");
	hi::filter(hm, hm2);
	fprintf(stderr, "after filter\n");
	hm2.dump_hdr();

	hi::threshold(hm2, hm, 150, 255);

	cv::Mat out;
	hm.download(out);
	save_mat(out, "saved/after_filter_threshold.x");

	fprintf(stderr, "..................\n");
	hm.dump_hdr();

	// 两次腐蚀 .
	hi::erode(hm, hm2);
	hi::erode(hm2, hm);
	hm.download(out);
	save_mat(out, "saved/after_threshold_erode.x");

	// 两次膨胀 ..
	hi::dilate(hm, hm2);
	hi::dilate(hm2, hm);
	hm.download(out);
	save_mat(out, "saved/after_threshold_dilate.x");

	// 积分 ..
	hi::integral(hm, hm2);
	cv::Mat im, imgray;
	hm2.download(im);	// im 为 32F

	double v0, v1;
	cv::minMaxLoc(im, &v0, &v1);
	fprintf(stderr, "v0=%.3f, v1=%.3f\n", v0, v1);
	im.convertTo(imgray, CV_8U, 255/(v1-v0), -v0*(255/(v1-v0)));
	save_mat(imgray, "saved/imgray.x");

#endif

	return 0;
}

