/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_hifb.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2011/10/15
  Description   : 
  History       :
  1.Date        : 2011/10/15
    Author      : s00187460
    Modification: Created file

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/mman.h>   //mmap
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>

#include "hi_common.h"
#include "hi_type.h"
#include "hi_comm_vb.h"
#include "hi_comm_sys.h"
#include "hi_comm_venc.h"
#include "hi_comm_vi.h"
#include "hi_comm_vo.h"
//#include "hi_comm_group.h"
#include "hi_comm_region.h"

#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "mpi_vi.h"
#include "mpi_vo.h"
#include "mpi_region.h"
#include "sample_comm.h"

#include <linux/fb.h>
#include "hifb.h"
#include "loadbmp.h"



static VI_DEV ViDev = 0;
static VI_CHN ViChn = 0;
static VO_DEV VoDev = 0;


void SAMPLE_VIO_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}


HI_S32 SAMPLE_HIFB_VO_Start(void)
{
#define HIFB_HD_WIDTH  1280
#define HIFB_HD_HEIGHT 720

    HI_S32 s32Ret = HI_SUCCESS;
    
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN_ATTR_S stChnAttr;

    HI_MPI_VO_Disable(VoDev);

    stPubAttr.enIntfType = VO_INTF_BT1120;
    stPubAttr.enIntfSync = VO_OUTPUT_720P50;
    stPubAttr.u32BgColor = 0xff0000ff;
       
    /* Attr of video layer */
    stLayerAttr.stDispRect.s32X       = 0;
    stLayerAttr.stDispRect.s32Y       = 0;
    stLayerAttr.stDispRect.u32Width   = HIFB_HD_WIDTH;
    stLayerAttr.stDispRect.u32Height  = HIFB_HD_HEIGHT;
    stLayerAttr.stImageSize.u32Width  = HIFB_HD_WIDTH;
    stLayerAttr.stImageSize.u32Height = HIFB_HD_HEIGHT;
    stLayerAttr.u32DispFrmRt          = 50;
    stLayerAttr.enPixFormat           = PIXEL_FORMAT_YUV_SEMIPLANAR_422;
   

    /* Attr of vo chn */    
    stChnAttr.stRect.s32X               = 0;
    stChnAttr.stRect.s32Y               = 0;
    stChnAttr.stRect.u32Width           = HIFB_HD_WIDTH;
    stChnAttr.stRect.u32Height          = HIFB_HD_HEIGHT;
    stChnAttr.bDeflicker                = HI_FALSE;
    stChnAttr.u32Priority               = 1;

    
    /* set public attr of VO*/
    if (HI_SUCCESS != HI_MPI_VO_SetPubAttr(VoDev, &stPubAttr))
    {
        printf("set VO pub attr failed !\n");
        return -1;
    }

    if (HI_SUCCESS != HI_MPI_VO_Enable(VoDev))
    {
        printf("enable vo device failed!\n");
        return -1;
    }

	s32Ret = HI_MPI_VO_SetVideoLayerAttr(VoDev, &stLayerAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("set video layer attr failed with %#x!\n", s32Ret);
        return -1;
    }

    if (HI_SUCCESS != HI_MPI_VO_EnableVideoLayer(VoDev))
    {
        printf("enable video layer failed!\n");
        return -1;
    }

    return 0;
}

HI_S32 SAMPLE_HIFB_VO_Stop(void)
{   
  if (HI_SUCCESS != HI_MPI_VO_DisableVideoLayer(VoDev))
    {
        printf("Disable video layer failed!\n");
        return -1;
    }

    if (HI_SUCCESS != HI_MPI_VO_Disable(VoDev))
    {
        printf("Disable vo device failed!\n");
        return -1;
    }    

    return 0;    
}

#define SAMPLE_IMAGE_WIDTH     184
#define SAMPLE_IMAGE_HEIGHT    144
#define SAMPLE_IMAGE_SIZE      (184*144*2)
#define SAMPLE_IMAGE_NUM       20
#if HICHIP == HI3531_V100
#define SAMPLE_IMAGE_PATH		"./res/%d.bmp"
#define SAMPLE_CURSOR_PATH		"./res/cursor.bmp"
#else HICHIP == HI3532_V100
#define SAMPLE_IMAGE_PATH		"/root/res/%d.bmp"
#define SAMPLE_CURSOR_PATH		"/root/res/cursor.bmp"
#endif
#define DIF_LAYER_NAME_LEN 20
#define HIL_MMZ_NAME_LEN 32
#define HIFB_RED_1555   0xFC00
#define SAMPLE_VIR_SCREEN_WIDTH	    SAMPLE_IMAGE_WIDTH			/*virtual screen width*/
#define SAMPLE_VIR_SCREEN_HEIGHT	SAMPLE_IMAGE_HEIGHT*2		/*virtual screen height*/
#define s32fd 0
#define HIL_MMB_NAME_LEN 16
#define g_s32fd  0


static struct fb_bitfield g_r16 = {10, 5, 0};
static struct fb_bitfield g_g16 = {5, 5, 0};
static struct fb_bitfield g_b16 = {0, 5, 0};
static struct fb_bitfield g_a16 = {15, 1, 0};

typedef enum 
{
    HIFB_LAYER_0 = 0x0,
    HIFB_LAYER_1,
    HIFB_LAYER_2,
    HIFB_LAYER_3,    
    HIFB_LAYER_4,
    HIFB_LAYER_CURSOR_0,
    HIFB_LAYER_CURSOR_1,
    //HIFB_LAYER_CURSOR,
    HIFB_LAYER_ID_BUTT
} HIFB_LAYER_ID_E;


typedef struct _LayerID_NAME_S
{
    HIFB_LAYER_ID_E     sLayerID;
    HI_CHAR             sLayerName[DIF_LAYER_NAME_LEN];     
}LayerID_NAME_S;

typedef struct hiPTHREAD_HIFB_SAMPLE
{
    int fd;
    int layer;
    int ctrlkey;
}PTHREAD_HIFB_SAMPLE_INFO;


HI_VOID *SAMPLE_HIFB_REFRESH(void *pData)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HIFB_LAYER_INFO_S stLayerInfo = {0};
    HIFB_BUFFER_S stCanvasBuf;
    HI_U16 *pBuf;
    HI_U32 x, y;
	HI_BOOL Show;
#if HICHIP == HI3531_V100
	HI_BOOL bCompress = HI_TRUE;
#endif
    HIFB_POINT_S stPoint = {0};
    struct fb_var_screeninfo stVarInfo;
	char file[12] = "/dev/fb0";
    HI_U32 maxW,maxH;
	PTHREAD_HIFB_SAMPLE_INFO *pstInfo;
	pstInfo = (PTHREAD_HIFB_SAMPLE_INFO *)pData;
	
	switch (pstInfo->layer)
	{
		case 0 :
			strcpy(file, "/dev/fb0");
			break;
		case 1 :
			strcpy(file, "/dev/fb1");
			break;
		case 2 :
			strcpy(file, "/dev/fb2");
			break;
		case 3 :
			strcpy(file, "/dev/fb3");
			break;
		case 4 :
			strcpy(file, "/dev/fb4");
			break;
		case 5 :
			strcpy(file, "/dev/fb5");
			break;
		case 6 :
			strcpy(file, "/dev/fb6");
			break;
		case 7 :
			strcpy(file, "/dev/fb7");
			break;
		default:
			strcpy(file, "/dev/fb0");
			break;
	}
	
		/* 1. open framebuffer device overlay 0 */
	pstInfo->fd = open(file, O_RDWR, 0);
	if(pstInfo->fd < 0)
	{
		printf("open %s failed!\n",file);
		return HI_NULL;
	} 
#if HICHIP == HI3531_V100

	if(pstInfo->layer >= HIFB_LAYER_0 && pstInfo->layer <= HIFB_LAYER_4)
	{  
		if (ioctl(pstInfo->fd, FBIOPUT_COMPRESSION_HIFB, &bCompress) < 0)
		{
			printf("FBIOPUT_COMPRESSION_HIFB failed!\n");
			close(pstInfo->fd);
			return HI_NULL;
		}
	}
#endif
    s32Ret = ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &stVarInfo);
	if(s32Ret < 0)
	{
		printf("GET_VSCREENINFO failed!\n");
		return HI_NULL;
	} 
	
    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
    {
        printf("set screen original show position failed!\n");
        return HI_NULL;
    }
    maxW = 1280;
	maxH = 720;
    stVarInfo.xres = stVarInfo.xres_virtual = maxW;
    stVarInfo.yres = stVarInfo.yres_virtual = maxH;
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &stVarInfo);
    if(s32Ret < 0)
	{
		printf("PUT_VSCREENINFO failed!\n");
		return HI_NULL;
	} 
    switch (pstInfo->ctrlkey)
	{
		case 0 :
		{  
			stLayerInfo.BufMode = HIFB_LAYER_BUF_ONE;
			stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
			break;
		}
		
		case 1 :
		{
			stLayerInfo.BufMode = HIFB_LAYER_BUF_DOUBLE;
		    stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
			break;
		}

		default:
		{
			stLayerInfo.BufMode = HIFB_LAYER_BUF_NONE;
			stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
		}
			
	}
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_LAYER_INFO, &stLayerInfo);
    if(s32Ret < 0)
	{
		printf("PUT_LAYER_INFO failed!\n");
		return HI_NULL;
	} 
	Show = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &Show) < 0)
    {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        return HI_NULL;
    }
	if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&(stCanvasBuf.stCanvas.u32PhyAddr), ((void**)&pBuf), 
            NULL, NULL, maxW*maxH*2))
    {
        printf("allocate memory (maxW*maxH*2 bytes) failed\n");
        return HI_NULL;
    }    
    stCanvasBuf.stCanvas.u32Height = maxH;
    stCanvasBuf.stCanvas.u32Width = maxW;
    stCanvasBuf.stCanvas.u32Pitch = maxW*2;
    stCanvasBuf.stCanvas.enFmt = HIFB_FMT_ARGB1555; 
	//printf("w:%d,h:%d,stride:%d\n", stCanvasBuf.stCanvas.u32Width, stCanvasBuf.stCanvas.u32Height, stCanvasBuf.stCanvas.u32Pitch);
    memset(pBuf, 0x8000, stCanvasBuf.stCanvas.u32Pitch*stCanvasBuf.stCanvas.u32Height);
	for (y = 358; y < 362; y++)
	{
		for (x = 0; x < 1280; x++)
		{
			*(pBuf + y * maxW + x) = HIFB_RED_1555;
		}
	}
	for (y = 0; y < 720; y++)
	{
		for (x = 638; x < 642; x++)
		{
			*(pBuf + y * maxW + x) = HIFB_RED_1555;
		}
	}
  
    stCanvasBuf.UpdateRect.x = 0;
    stCanvasBuf.UpdateRect.y = 358;
    stCanvasBuf.UpdateRect.w = maxW;
    stCanvasBuf.UpdateRect.h = 5;  
    s32Ret = ioctl(pstInfo->fd, FBIO_REFRESH, &stCanvasBuf);
    if(s32Ret < 0)
	{
		printf("REFRESH failed!\n");
		HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);    
		return HI_NULL;
	}
	
    stCanvasBuf.UpdateRect.x = 638;
    stCanvasBuf.UpdateRect.y = 0;
    stCanvasBuf.UpdateRect.w = 5;
    stCanvasBuf.UpdateRect.h = maxH; 
	
    s32Ret = ioctl(pstInfo->fd, FBIO_REFRESH, &stCanvasBuf);
    if(s32Ret < 0)
	{
		printf("REFRESH failed!\n");
		HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);    
		return HI_NULL;
	} 
	usleep(40* 1000);
    printf("expected:two red  line!\n");
    sleep(25);
    HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);    
    close(pstInfo->fd);
	return HI_NULL;   
}


HI_S32 SAMPLE_HIFB_LoadBmp(const char *filename, HI_U8 *pAddr)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if(GetBmpInfo(filename,&bmpFileHeader,&bmpInfo) < 0)
    {
		printf("GetBmpInfo err!\n");
        return HI_FAILURE;
    }
    
    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;

    CreateSurfaceByBitMap(filename,&Surface,pAddr);
    
    return HI_SUCCESS;
}

HI_VOID *SAMPLE_HIFB_PTHREAD_RunHiFB(void *pData)
{
    HI_S32 i;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    HI_U32 u32FixScreenStride = 0;
    unsigned char *pShowScreen;
    unsigned char *pHideScreen;
    HIFB_ALPHA_S stAlpha;
    HIFB_POINT_S stPoint = {40, 112};
    char file[12] = "/dev/fb0";
#if HICHIP == HI3531_V100
	HI_BOOL g_bCompress = HI_TRUE;
#endif
    char image_name[128];
	HI_U8 *pDst = NULL;
    HI_BOOL bShow;
    PTHREAD_HIFB_SAMPLE_INFO *pstInfo;
	HIFB_COLORKEY_S stColorKey;

    if(HI_NULL == pData)
    {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO *)pData;
    switch (pstInfo->layer)
    {
        case 0 :
            strcpy(file, "/dev/fb0");
            break;
        case 1 :
            strcpy(file, "/dev/fb1");
            break;
        case 2 :
            strcpy(file, "/dev/fb2");
            break;
        case 3 :
            strcpy(file, "/dev/fb3");
            break;
        case 4 :
            strcpy(file, "/dev/fb4");
            break;
        case 5 :
            strcpy(file, "/dev/fb5");
            break;
        case 6 :
            strcpy(file, "/dev/fb6");
            break;
        case 7 :
            strcpy(file, "/dev/fb7");
            break;
        default:
            strcpy(file, "/dev/fb0");
            break;
    }

    /* 1. open framebuffer device overlay 0 */
    pstInfo->fd = open(file, O_RDWR, 0);
    if(pstInfo->fd < 0)
    {
        printf("open %s failed!\n",file);
        return HI_NULL;
    } 
#if HICHIP == HI3531_V100
	if(pstInfo->layer >= HIFB_LAYER_0 && pstInfo->layer <= HIFB_LAYER_4)
	{
		if (ioctl(pstInfo->fd, FBIOPUT_COMPRESSION_HIFB, &g_bCompress) < 0)
		{
			printf("Func:%s line:%d FBIOPUT_COMPRESSION_HIFB failed!\n", 
			__FUNCTION__, __LINE__);
			close(pstInfo->fd);
			return HI_NULL;
		}
	}
#endif
    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        return HI_NULL;
    }
    /* 2. set the screen original position */
    switch(pstInfo->ctrlkey)
    {
        case 0:
        {
            stPoint.s32XPos= 100;
            stPoint.s32YPos = 100;
        }
        break;
        
        case 1:
        {
            stPoint.s32XPos = 150;
            stPoint.s32YPos = 350;
        }
        break;
        
        case 2:
        {
            stPoint.s32XPos = 384;
            stPoint.s32YPos = 100;
        }
        break;
        case 3:
        {
            stPoint.s32XPos = 150;
            stPoint.s32YPos = 150;
        }
		break;
        default:
        {
			stPoint.s32XPos = 0;
            stPoint.s32YPos = 0;
        }
    }
    
    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
    {
        printf("set screen original show position failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }

    /* 3.set alpha */
    stAlpha.bAlphaEnable = HI_TRUE;
    stAlpha.bAlphaChannel = HI_TRUE;
    stAlpha.u8Alpha0 = 0xff;
    stAlpha.u8Alpha1 = 0xff;
    stAlpha.u8GlobalAlpha = 0x80;
    if (ioctl(pstInfo->fd, FBIOPUT_ALPHA_HIFB,  &stAlpha) < 0)
    {
        printf("Set alpha failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }
    if(pstInfo->layer == HIFB_LAYER_CURSOR_0 || pstInfo->layer == HIFB_LAYER_CURSOR_1)
	{	  
	  stColorKey.bKeyEnable = HI_TRUE;
	  stColorKey.u32Key = 0x0;
	  if (ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey) < 0)
	  {
		printf("FBIOPUT_COLORKEY_HIFB!\n");
		close(pstInfo->fd);
        return HI_NULL;
	  }	  
    }
    /* 4. get the variable screen info */
    if (ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &var) < 0)
    {
        printf("Get variable screen info failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }

    /* 5. modify the variable screen info
          the screen size: IMAGE_WIDTH*IMAGE_HEIGHT
          the virtual screen size: VIR_SCREEN_WIDTH*VIR_SCREEN_HEIGHT
          (which equals to VIR_SCREEN_WIDTH*(IMAGE_HEIGHT*2))
          the pixel format: ARGB1555
    */
    usleep(4*1000*1000);
    switch(pstInfo->ctrlkey)
    {
        case 0:
        {
            var.xres_virtual = 104;
            var.yres_virtual = 200;
            var.xres = 104;
            var.yres = 100;
        }
        break;
        
        case 1:
        {
            var.xres_virtual = 100;
            var.yres_virtual = 100;
            var.xres = 100;
            var.yres = 100;
        }
        break;
        
        case 2:
        {
            var.xres_virtual = SAMPLE_VIR_SCREEN_WIDTH;
            var.yres_virtual = SAMPLE_VIR_SCREEN_HEIGHT;
            var.xres = SAMPLE_IMAGE_WIDTH;
            var.yres = SAMPLE_IMAGE_HEIGHT;
        }
        break;
        case 3:
        {
            var.xres_virtual = 48;
            var.yres_virtual = 48;
            var.xres = 48;
            var.yres = 48;
        }
        break;
        default:
        {
            var.xres_virtual = 98;
            var.yres_virtual = 128;
            var.xres = 98;
            var.yres = 64;
        }
    }

    var.transp= g_a16;
    var.red = g_r16;
    var.green = g_g16;
    var.blue = g_b16;
    var.bits_per_pixel = 16;
    var.activate = FB_ACTIVATE_NOW;
    
    /* 6. set the variable screeninfo */
    if (ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &var) < 0)
    {
        printf("Put variable screen info failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }

    /* 7. get the fix screen info */
    if (ioctl(pstInfo->fd, FBIOGET_FSCREENINFO, &fix) < 0)
    {
        printf("Get fix screen info failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }
    u32FixScreenStride = fix.line_length;   /*fix screen stride*/

    /* 8. map the physical video memory for user use */
    pShowScreen = mmap(HI_NULL, fix.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, pstInfo->fd, 0);
    if(MAP_FAILED == pShowScreen)
    {
        printf("mmap framebuffer failed!\n");
		close(pstInfo->fd);
        return HI_NULL;
    }

    memset(pShowScreen, 0x83E0, fix.smem_len);

    /* time to paly*/
    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        munmap(pShowScreen, fix.smem_len);
        return HI_NULL;
    }  		
    switch(pstInfo->ctrlkey)
    {
        case 0:
        {
            /*change color*/
            pHideScreen = pShowScreen + u32FixScreenStride*var.yres;
            memset(pHideScreen, 0x8000, u32FixScreenStride*var.yres);
            memset(pShowScreen, 0x7c00,  u32FixScreenStride*var.yres);
            for(i=0; i<SAMPLE_IMAGE_NUM; i++) //IMAGE_NUM
            {
                if(i%2)
                {
                    var.yoffset = 0;
                }
                else
                {
                    var.yoffset = var.yres;
                }

                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0)
                {
                    printf("FBIOPAN_DISPLAY failed!\n");
                    munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
                    return HI_NULL;
                }

                usleep(1000*1000);
            }
        }
        break;
        
        case 1:
        {
            /*move*/
			HI_U32 u32PosXtemp;
			u32PosXtemp = stPoint.s32XPos;	
			
            for(i=0;i<400;i++)
            {
                if(i > 200)
                {
                    stPoint.s32XPos = u32PosXtemp + i%20;
                    stPoint.s32YPos--;
                }
                else
                {
                    stPoint.s32XPos = u32PosXtemp - i%20;
                    stPoint.s32YPos++;
                }                
                if(ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
                {
                    printf("set screen original show position failed!\n");
					munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
                    return HI_NULL;
                }

                usleep(70*1000);				
            }
		}
        break;
        
        case 2:
        {
            /*change bmp*/
            pHideScreen = pShowScreen + u32FixScreenStride*SAMPLE_IMAGE_HEIGHT;
            memset(pShowScreen, 0, u32FixScreenStride*SAMPLE_IMAGE_HEIGHT*2);         
		    for(i = 0; i < SAMPLE_IMAGE_NUM; i++)
			{ 
				stPoint.s32XPos = stPoint.s32XPos + 4;
				stPoint.s32YPos = stPoint.s32YPos - 4;
				if(ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
				{
					printf("set screen original show position failed!\n");
					munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
					return HI_NULL;
				}
				sprintf(image_name, SAMPLE_IMAGE_PATH, i%2);
                pDst = (HI_U8 *)pHideScreen;
                SAMPLE_HIFB_LoadBmp(image_name,pDst);
                if(i%2)
                {
                    var.yoffset = 0;
                    pHideScreen = pShowScreen + u32FixScreenStride*SAMPLE_IMAGE_HEIGHT;
                }
                else
                {
                    var.yoffset = SAMPLE_IMAGE_HEIGHT;
                    pHideScreen = pShowScreen;
                }
                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0)
                {
                    printf("FBIOPAN_DISPLAY failed!\n");
                    munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
                    return HI_NULL;
                }                
               usleep(500*1000);				
            }   
        }
        break;

		case 3:
		{
			/* move cursor */
			SAMPLE_HIFB_LoadBmp(SAMPLE_CURSOR_PATH,pShowScreen);
			if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0)
			{
				printf("FBIOPAN_DISPLAY failed!\n");
				munmap(pShowScreen, fix.smem_len);
				close(pstInfo->fd);
				return HI_FALSE;
			}    
			printf("show cursor\n");
			sleep(2);
			for(i=0;i<100;i++)
			{		
				stPoint.s32XPos += 2;
				stPoint.s32YPos += 2;	
				if(ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
				{
					printf("set screen original show position failed!\n");
					munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
					return HI_FALSE;
				}
				usleep(70*1000);
			}
			for(i=0;i<100;i++)
			{		
				stPoint.s32XPos += 2;
				stPoint.s32YPos -= 2;	
				if(ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
				{
					printf("set screen original show position failed!\n");
					munmap(pShowScreen, fix.smem_len);
					close(pstInfo->fd);
					return HI_NULL;
				}
				usleep(70*1000);
			}		
			printf("move the cursor\n");
			sleep(1);
			HI_MPI_VO_GfxLayerUnBindDev(GRAPHICS_LAYER_HC0, VoDev);
		}
		break;	
        default:
        {
        }   
    }
    
    /* unmap the physical memory */
    munmap(pShowScreen, fix.smem_len);

    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
	{
		printf("FBIOPUT_SHOW_HIFB failed!\n");
		return HI_NULL;
	}
    close(pstInfo->fd);
    return HI_NULL;
}


int main(int argc, char *argv[])
{
	pthread_t phifb0;
#if HICHIP == HI3531_V100	
	pthread_t phifb1;
	PTHREAD_HIFB_SAMPLE_INFO stInfo1;
	VOU_GFX_BIND_LAYER_E enGfxBindLayer; 
#endif
	pthread_t phifb2;
	PTHREAD_HIFB_SAMPLE_INFO stInfo0;
	
	PTHREAD_HIFB_SAMPLE_INFO stInfo2;
	VO_PUB_ATTR_S stPubAttr;
	VB_CONF_S stVbConf;
	HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 i;
	//SAMPLE_VI_DEV_TYPE_E enViType = VI_DEV_BT656_D1_4MUX;
    SAMPLE_VI_MODE_E enViMode = SAMPLE_VI_MODE_1_D1;
	SAMPLE_VO_MODE_E stVoMode = VO_MODE_4MUX;
	HI_U32 u32ViChnNum = 4;
	HI_U32 u32ViDevNum;
	
	memset(&stVbConf, 0, sizeof(VB_CONF_S));
    stVbConf.u32MaxPoolCnt             = 16;
	stVbConf.astCommPool[0].u32BlkSize = 768*576*2;
	stVbConf.astCommPool[0].u32BlkCnt  = 16;
	
	stPubAttr.u32BgColor = 0xff00ff00;
#if HICHIP == HI3531_V100
	stPubAttr.enIntfType = VO_INTF_BT1120 | VO_INTF_HDMI;
#else
    stPubAttr.enIntfType = VO_INTF_BT1120;
#endif
    stPubAttr.enIntfSync = VO_OUTPUT_720P50;
	stPubAttr.bDoubleFrame = HI_FALSE;
	
	signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);
    
	if(HI_SUCCESS != SAMPLE_COMM_SYS_Init(&stVbConf))
	{
		printf("func:%s,line:%d\n", __FUNCTION__, __LINE__);
		return -1;	
	}

    s32Ret = SAMPLE_COMM_VI_Start(enViMode, VIDEO_ENCODING_MODE_PAL);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: Start Vi failed!\n", __FUNCTION__);
		SAMPLE_COMM_SYS_Exit();
        return -1;
    }
    
    /* start VO to preview                                                     */
	if(HI_SUCCESS != SAMPLE_COMM_VO_StartDevLayer(VoDev, &stPubAttr,25))
	{	
		printf("%s: Start DevLayer failed!\n", __FUNCTION__);
		SAMPLE_COMM_SYS_Exit();
		return -1;
	}
	
    if(HI_SUCCESS != SAMPLE_COMM_VO_StartChn(VoDev, &stPubAttr, stVoMode))
	{	
		printf("%s: Start VOChn failed!\n", __FUNCTION__);
		SAMPLE_COMM_SYS_Exit();
		return -1;
	}

    /* if it's displayed on HDMI, we should start HDMI */
    if (stPubAttr.enIntfType & VO_INTF_HDMI)
    {
        if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stPubAttr.enIntfSync))
        {
            printf("%s: Start HDMI failed!\n", __FUNCTION__);
    		SAMPLE_COMM_SYS_Exit();
    		return -1;
        }
    }
    
	for(i=0; i<4; i++)
	{
        s32Ret = SAMPLE_COMM_VO_BindVi(VoDev, i, i);
		if (HI_SUCCESS != s32Ret)
		{	
			printf("%s: VI Bind to VO failed!\n", __FUNCTION__);
			SAMPLE_COMM_SYS_Exit();
			return -1;
		}
	}
    
    /*start hifb                       */    
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  0;
    pthread_create(&phifb0,0,SAMPLE_HIFB_REFRESH,(void *)(&stInfo0));
	
#if HICHIP == HI3531_V100	
    for (enGfxBindLayer = GRAPHICS_LAYER_G4; enGfxBindLayer < GRAPHICS_LAYER_BUTT; ++enGfxBindLayer)
	{
		HI_MPI_VO_GfxLayerUnBindDev(enGfxBindLayer, VoDev);
	}
	if (HI_SUCCESS != HI_MPI_VO_GfxLayerBindDev(GRAPHICS_LAYER_G4, VoDev))
	{
		printf("%s: Graphic Bind to VODev failed!,line:%d\n", __FUNCTION__,  __LINE__);
		SAMPLE_COMM_SYS_Exit();
		SAMPLE_HIFB_VO_Stop();
		return -1;
	}

	stInfo1.layer   =  4;
	stInfo1.fd      = -1;
	stInfo1.ctrlkey =  2;
    pthread_create(&phifb1, 0, SAMPLE_HIFB_PTHREAD_RunHiFB, (void *)(&stInfo1));
#endif
	stInfo2.layer   =  5;
	stInfo2.fd      = -1;
	stInfo2.ctrlkey =  3;
	
#if HICHIP == HI3531_V100
	if (HI_SUCCESS != HI_MPI_VO_GfxLayerBindDev(GRAPHICS_LAYER_HC0, VoDev))
	{
		printf("%s: Graphic Bind to VODev failed!,line:%d\n", __FUNCTION__, __LINE__);
		SAMPLE_COMM_SYS_Exit();
		SAMPLE_HIFB_VO_Stop();
		return -1;
	}
#endif
    pthread_create(&phifb2,0,SAMPLE_HIFB_PTHREAD_RunHiFB,(void *)(&stInfo2));
    pthread_join(phifb0,0);
#if HICHIP == HI3531_V100
    pthread_join(phifb1,0);
#endif
    pthread_join(phifb2,0);	

    for(i=0;i<4;i++)
    {
        SAMPLE_COMM_VO_UnBindVi(VoDev,i);
    }
    
    SAMPLE_COMM_VO_StopChn(VoDev, stVoMode);
    SAMPLE_COMM_VO_StopDevLayer(VoDev);
    if (stPubAttr.enIntfType & VO_INTF_HDMI)
    {
        SAMPLE_COMM_VO_HdmiStop();
    }
    
    SAMPLE_COMM_VI_Stop(enViMode);
    
    /*mpi exit */
	HI_MPI_SYS_Exit();
	HI_MPI_VB_Exit();
    return 0;
}





