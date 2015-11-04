#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_ALIGN_WIDTH 16
#define PRT_ERR(fmt...) \
	do { \
		fprintf(stderr, "[%s]-%d: ", __FUNCTION__, __LINE__); \
		fprintf(stderr, fmt); \
	} while(0)

#define PIXEL_FORMAT         PIXEL_FORMAT_YUV_SEMIPLANAR_420

#define VI_SUBCHN_2_W 720
#define VI_SUBCHN_2_H  640
typedef struct hi31_t hi31_t;

typedef struct point_t
{
	int x;
	int y;
} POINT;

typedef struct size
{
	unsigned int width;
	unsigned int height;
} SIZE;

typedef struct rect 
{
	int x;
	int y;
	int width;
	int height;
} RECT;

typedef struct region
{
	int st;
	int at;
	int ot;
	int ut;
	RECT rect;
} REGION;

typedef enum vi_mode_e
{
	VI_MODE_16_Cif,
	VI_MODE_4_720P,
	VI_MODE_4_1080P
} VI_MODE_E;

typedef struct vdas
{
	int num;
	SIZE size;
	REGION regions[4];
	const char* image_file;
} VDAS;

typedef struct chns
{
	int vi_chn;
	int vda_chn;
} CHNS;

typedef struct td
{
	double stamp;
	int is_alarms[4];
	RECT rects[4];
} TD;


typedef struct hi3531_ptrs
{
	CHNS chns;
	VDAS vdas;
} HI31_PS;

typedef struct vi_param_s
{
    int s32ViDevCnt;		// VI Dev Total Count
    int s32ViDevInterval;	// Vi Dev Interval
    int s32ViChnCnt;		// Vi Chn Total Count
    int s32ViChnInterval;	// VI Chn Interval
}VI_PARAM_S;

typedef enum vi_chn_set_e
{
    VI_CHN_SET_NORMAL = 0, /* mirror, filp close */
    VI_CHN_SET_MIRROR,      /* open MIRROR */
    VI_CHN_SET_FILP		/* open filp */
} VI_CHN_SET_E;

int open_hi3531(hi31_t **hi31, HI31_PS ps);

int read_hi3531(hi31_t *hi31, TD *td);

void  close_hi3531(hi31_t *hi31);


#ifdef __cplusplus
}
#endif
