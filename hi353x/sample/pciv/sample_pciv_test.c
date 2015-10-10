#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "hi_comm_pciv.h"
#include "mpi_pciv.h"
#include "hi_comm_sys.h"
#include "mpi_sys.h"
#include "pciv_msg.h"
#include "pciv_trans.h"
#include "sample_pciv_comm.h"

#include "loadbmp.h"
#include "hi_comm_vpp.h"
#include "mpi_vpp.h"


/* 存储 Packet VYUY422 格式为 Planar422 格式*/
HI_VOID SAMPLE_PCIV_SavePacketToPlanar(FILE *pfile, 
    HI_U32 u32Width, HI_U32 u32Height, HI_U8 *pu8Addr)
{
	unsigned int w, h;
	char * pVBufVirt_Y;
	char * pu8PixPtr= NULL;
	unsigned char TmpBuff[2048];
	HI_U32 u32UvHeight;/* 存为planar 格式时的UV分量的高度 */

	printf("\n~~~~~~~~~~~SavePacketToPlanar w:%d h:%d paddr =%p~~~~~~~~~~~~~~\n",
	    u32Width, u32Height, pu8Addr);

    u32UvHeight = u32Height;
	pVBufVirt_Y = pu8Addr; 

	/* save Y ----------------------------------------------------------------*/
	printf("saving......Y......");
	/*packet YUYV: |31--------0|
	                Y1 U0 Y0 V0 */ 
	pu8PixPtr = pVBufVirt_Y + 1;  /*此时pu8PixPtr指向Y0*/
	for(h=0; h<u32UvHeight; h++)
	{   
	    for(w=0; w<u32Width; w++)
	    {
	        TmpBuff[w] = *pu8PixPtr;
	        pu8PixPtr += 2;
	    }
	    fwrite(TmpBuff, 1, u32Width, pfile);
	}

	/* save U ----------------------------------------------------------------*/
	printf("U......");
	pu8PixPtr = pVBufVirt_Y + 2 ;  /*此时pu8PixPtr指向U0*/
	for(h=0; h<u32UvHeight; h++)
	{
	    for(w=0; w<u32Width/2; w++)
	    {
	        TmpBuff[w] = *pu8PixPtr;
	        pu8PixPtr += 4;
	    }
	    fwrite(TmpBuff, 1, u32Width/2, pfile);
	}

	/* save V ----------------------------------------------------------------*/
	printf("V......");
	pu8PixPtr = pVBufVirt_Y ;  /*此时pu8PixPtr指向V0*/
	for(h=0; h<u32UvHeight; h++)    
	{
	    for(w=0; w<u32Width/2; w++)
	    {
	        TmpBuff[w] = *pu8PixPtr;
	        pu8PixPtr += 4;
	    }
	    fwrite(TmpBuff, 1, u32Width/2, pfile);
	}
    
    printf("\n");
	return;
}


HI_S32 SAMPLE_PCIV_SaveFile(PCIV_CHN PcivChn)
{
    HI_S32 i;
    PCIV_ATTR_S stPcivAttr;
    HI_U8 *pu8Addr[8];
    FILE *pFile[8];
    HI_CHAR aszFileName[8][128];
    
    if (HI_MPI_PCIV_GetAttr(PcivChn, &stPcivAttr))
    {
        printf("get pciv attr failed \n");        
        return -1;
    }

    /* 映射内存 */
    for (i=0; i<stPcivAttr.u32Count; i++)
    {
        pu8Addr[i] = HI_MPI_SYS_Mmap(stPcivAttr.u32PhyAddr[i], stPcivAttr.u32BlkSize);
        if (!pu8Addr[i])
        {
            printf("mmap failed \n");
            return -1;
        }
    }

    /* 创建新文件 */
    for (i=0; i<stPcivAttr.u32Count; i++)
    {
        sprintf(aszFileName[i], "pciv%d_%d_%d_%d.yuv", PcivChn, 
            stPcivAttr.stPicAttr.u32Width, stPcivAttr.stPicAttr.u32Height, i); 
        pFile[i] = fopen(aszFileName[i], "w+");
        if (NULL == pFile[i])
        {
            printf("open file %s err\n", aszFileName[i]);
            return -1;
        }
    }

    if (stPcivAttr.stPicAttr.enPixelFormat == PIXEL_FORMAT_VYUY_PACKAGE_422)
    {
        /* Packge 格式转存为Planar 格式 */
        for (i=0; i<stPcivAttr.u32Count; i++)
        {
            SAMPLE_PCIV_SavePacketToPlanar(pFile[i], stPcivAttr.stPicAttr.u32Width,
                    stPcivAttr.stPicAttr.u32Height, pu8Addr[i]);
        }
    }
    else 
    {
        printf("not support save this pix format \n");
        return -1;
    }
    

    /* 释放资源 */
    for (i=0; i<stPcivAttr.u32Count; i++)
    {
        fclose(pFile[i]);
        HI_MPI_SYS_Munmap(pu8Addr[i], stPcivAttr.u32BlkSize);
    }
    
    return HI_SUCCESS;
}

HI_S32 SamplePcivLoadBmp(const char *filename, REGION_CTRL_PARAM_U *pParam)
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

    pParam->stBitmap.pData = malloc(2*(bmpInfo.bmiHeader.biWidth)*(bmpInfo.bmiHeader.biHeight));

    if(NULL == pParam->stBitmap.pData)
    {
        printf("malloc osd memroy err!\n");
        return HI_FAILURE;
    }

    CreateSurfaceByBitMap(filename,&Surface,(HI_U8*)(pParam->stBitmap.pData));

    pParam->stBitmap.u32Width = Surface.u16Width;
    pParam->stBitmap.u32Height = Surface.u16Height;
    pParam->stBitmap.enPixelFormat = PIXEL_FORMAT_RGB_1555;

    return HI_SUCCESS;
}

void *SamplePcivViOverlayProc(void *parg)
{
    int i, s32Ret;
    REGION_CRTL_CODE_E enCtrl;
	REGION_CTRL_PARAM_U unParam;
    REGION_HANDLE RgnHnd = *(REGION_HANDLE*)parg;

    for (i=0; i<100; i++)
    {
        enCtrl = (i%2) ? REGION_SHOW : REGION_HIDE;
    	s32Ret = HI_MPI_VPP_ControlRegion(RgnHnd, enCtrl, &unParam);
		if (s32Ret)
		{
            printf("HI_MPI_VPP_ControlRegion err, handle:%d, ctl:%d \n", RgnHnd, enCtrl);
            return NULL;
		}
        
        usleep(100*1000);
    }
    return NULL;
}
HI_S32 SAMPLE_PCIV_ViOverlay(VI_DEV ViDev, VI_CHN ViChn, HI_CHAR *pszBmpFile)
{
    HI_S32 i = 0;
	HI_S32 s32Ret = 0;
	REGION_CRTL_CODE_E enCtrl;
	REGION_CTRL_PARAM_U unParam;
	REGION_ATTR_S stRgnAttr;
	REGION_HANDLE handle[MAX_VIOVERLAY_NUM] = {0};

    s32Ret = SamplePcivLoadBmp(pszBmpFile, &unParam);
    if(HI_SUCCESS != s32Ret)
    {
        printf("Load bmp 2 YUV err 0x%x\n",s32Ret);
		return HI_FAILURE;
    }

	stRgnAttr.enType = OVERLAYEX_RGN;
	stRgnAttr.unAttr.stOverlayEx.bIsPublic = HI_FALSE;
	stRgnAttr.unAttr.stOverlayEx.stRect.s32X = 50;
	stRgnAttr.unAttr.stOverlayEx.stRect.s32Y = 50;
	stRgnAttr.unAttr.stOverlayEx.stRect.u32Width = unParam.stBitmap.u32Width;
	stRgnAttr.unAttr.stOverlayEx.stRect.u32Height = unParam.stBitmap.u32Height;
	stRgnAttr.unAttr.stOverlayEx.u32Layer = 0;
	stRgnAttr.unAttr.stOverlayEx.u32BgColor = 0x1<<15;
    stRgnAttr.unAttr.stOverlayEx.u8GlobalAlpha = 255;
    stRgnAttr.unAttr.stOverlayEx.bAlphaExt1555 = HI_FALSE;
    stRgnAttr.unAttr.stOverlayEx.u8Alpha0 = 255;
    stRgnAttr.unAttr.stOverlayEx.u8Alpha1 = 255;
	stRgnAttr.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_RGB_1555;
    stRgnAttr.unAttr.stCover.ViDevId = ViDev;
	stRgnAttr.unAttr.stCover.ViChn = ViChn;
	s32Ret = HI_MPI_VPP_CreateRegion(&stRgnAttr, &handle[0]);
	if(s32Ret != HI_SUCCESS)
	{
		printf("HI_MPI_VPP_CreateRegion err 0x%x\n",s32Ret);
		return HI_FAILURE;
	}

	/*insert YUV to region and show region*/
	for(i = 0; i<1; i++)
	{
	    enCtrl = REGION_SET_BITMAP;
	    s32Ret = HI_MPI_VPP_ControlRegion(handle[i],enCtrl,&unParam);
    	if(s32Ret != HI_SUCCESS)
    	{
    	    if(unParam.stBitmap.pData != NULL)
			{
				free(unParam.stBitmap.pData);
				unParam.stBitmap.pData = NULL;
			}
    		printf("setbitmap faild 0x%x!!!\n",s32Ret);
    		return HI_FAILURE;
    	}

    	enCtrl = REGION_SHOW;
    	s32Ret = HI_MPI_VPP_ControlRegion(handle[i],enCtrl,&unParam);
    	if(s32Ret != HI_SUCCESS)
    	{
    		printf("show faild 0x%x!!!\n",s32Ret);
    		return HI_FAILURE;
    	}
    }

    if (1)
    {
        pthread_t stPid;
        pthread_attr_t stAttr;

        /* 以分离状态方式创建线程 */
        pthread_attr_init(&stAttr);
        pthread_attr_setdetachstate(&stAttr ,PTHREAD_CREATE_DETACHED);
        pthread_create(&stPid, &stAttr, SamplePcivViOverlayProc, &handle[0]);
        pthread_attr_destroy(&stAttr);
    }    
    
    printf("vi(%d, %d) add overlay ok \n", ViDev, ViChn);
    return HI_SUCCESS;
}

HI_S32 SAMPLE_PCIV_SlaveTest(PCIV_CHN PcivChn)
{
    HI_S32 s32Ret, u32Count;
    PCIV_PREPROC_CFG_S stCfg;
    PCIV_ATTR_S stPcivAttr;

    s32Ret = HI_MPI_PCIV_GetAttr(PcivChn, &stPcivAttr);
    PCIV_CHECK_ERR(s32Ret); 

    /* 设置前处理属性 */ 
    if (0) {
        s32Ret = HI_MPI_PCIV_GetPreProcCfg(PcivChn, &stCfg);
        PCIV_CHECK_ERR(s32Ret); 
        printf("PreProcCfg=> enFilterType:%d, enHFilter:%d, enVFilterH:%d, enVFilterL:%d,  enFieldSel:%d\n", 
            stCfg.enFilterType, stCfg.enHFilter, stCfg.enVFilterC, stCfg.enVFilterL, stCfg.enFieldSel);
        
        stCfg.enFilterType = PCIV_FILTER_TYPE_EX2;
        stCfg.enFieldSel = PCIV_FIELD_BOTTOM;
        s32Ret = HI_MPI_PCIV_SetPreProcCfg(PcivChn, &stCfg);
        printf("PreProcCfg=> enFilterType:%d, enHFilter:%d, enVFilterH:%d, enVFilterL:%d, enFieldSel:%d \n", 
            stCfg.enFilterType, stCfg.enHFilter, stCfg.enVFilterC, stCfg.enVFilterL, stCfg.enFieldSel);
        PCIV_CHECK_ERR(s32Ret); 
    }

    /* 叠加 ViOverlay */
    if (1) {
        PCIV_BIND_OBJ_S astBindObj[PCIV_MAX_BINDOBJ];
        
        s32Ret = HI_MPI_PCIV_EnumBindObj(PcivChn, astBindObj, &u32Count);
        PCIV_CHECK_ERR(s32Ret);
        
        if (astBindObj[0].enType == PCIV_BIND_VI)
        {
            VI_DEV ViDev = astBindObj[0].unAttachObj.viDevice.viDev;
            VI_CHN ViChn = astBindObj[0].unAttachObj.viDevice.viChn;
            s32Ret = SAMPLE_PCIV_ViOverlay(ViDev, ViChn, "mm.bmp");
            PCIV_CHECK_ERR(s32Ret);
        }
    }

    return HI_SUCCESS;
}
