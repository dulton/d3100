/******************************************************************************
  A simple program of Hisilicon HI3516 vda implementation.
  the flow as follows:
    1) init mpp system.
    2) start vi( internal isp, ViDev 0, vichn0) and vo (HD)                  
    3) vda md & od start & print information
    4) stop vi vo and system.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-2 Created
******************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "sample_comm.h"
#include "loadbmp.h"

VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;
HI_U32    gs_u32ViFrmRate = 0;
/******************************************************************************
* function : to process abnormal case                                        
******************************************************************************/
void SAMPLE_VDA_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}

/******************************************************************************
* function      : main() 
* Description : Vi/VO + VDA(MD&OD)
*               DC -> VI-PortA ViChn0(1080p) -> VO HD
*                              ViChn1(D1)    -> VdaChn0 MD
*                                            -> VdaChn1 OD
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    VI_CHN ViChn, ViChn_Md = 0, ViChn_Od = 0;
    VDA_CHN VdaChn_Md = 0, VdaChn_Od = 1;
    VB_CONF_S stVbConf ={0};	/* vb config define */
    PIC_SIZE_E enSize_Md = PIC_CIF, enSize_Od = PIC_CIF; 	/* vda picture size */

    SAMPLE_VI_MODE_E enViMode = SAMPLE_VI_MODE_1_D1;
    HI_U32 u32ViChnCnt = 1;
    
    VO_DEV VoDev;
    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;
    
    HI_S32 i;
    HI_U32 u32BlkSize;
    SIZE_S stSize;
    HI_U32 u32WndNum;

    /******************************************
     step  1: init global  variable 
    ******************************************/
    gs_u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL== gs_enNorm)?25:30;
    
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;

    /*ddr0 video buffer*/
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = u32ViChnCnt * 10;
    memset(stVbConf.astCommPool[0].acMmzName,0,
        sizeof(stVbConf.astCommPool[0].acMmzName));

    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END0;
    }

    s32Ret = SAMPLE_COMM_VI_MemConfig(enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_MemConfig failed with %d!\n", s32Ret);
        goto END0;
    }
    
    s32Ret = SAMPLE_COMM_VO_MemConfig(SAMPLE_VO_DEV_DSD0, NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_MemConfig failed with %d!\n", s32Ret);
        goto END0;
    }

    /******************************************
     step 3: start vi dev & chn to capture
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_Start(enViMode, gs_enNorm);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto END0;
    }
    
    /******************************************
     step 5: start VO to preview
    ******************************************/
#if HICHIP == HI3531_V100 
    VoDev = SAMPLE_VO_DEV_DSD0;
    u32WndNum = 1;
    enVoMode = VO_MODE_1MUX;
    
    stVoPubAttr.enIntfSync = VO_OUTPUT_PAL;
    stVoPubAttr.enIntfType = VO_INTF_CVBS;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;
#else
    /******************************************
         step 5: start VO to preview
    ******************************************/
    VoDev = SAMPLE_VO_DEV_DHD0;
    u32WndNum = 1;
    enVoMode = VO_MODE_1MUX;

    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P50;
    stVoPubAttr.enIntfType = VO_INTF_BT1120;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;
#endif
    s32Ret = SAMPLE_COMM_VO_StartDevLayer(VoDev, &stVoPubAttr, gs_u32ViFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDevLayer failed!\n");
        goto END0;
    }
    
    s32Ret = SAMPLE_COMM_VO_StartChn(VoDev, &stVoPubAttr, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartChn failed!\n");
        goto END0;
    }

    for(i=0;i<u32WndNum;i++)
    {
        VoChn = i;
        ViChn = i;
        s32Ret = SAMPLE_COMM_VO_BindVi(VoDev,VoChn,ViChn);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VO_BindVi failed!\n");
            goto END0;
        }
    }

    /******************************************
     step  3: VDA process
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize_Md, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END0;
    }
    s32Ret = SAMPLE_COMM_VDA_MdStart(VdaChn_Md, ViChn_Md, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VDA Md Start failed!\n");
        goto END0;
    }
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize_Od, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END0;
    }
    s32Ret = SAMPLE_COMM_VDA_OdStart(VdaChn_Od, ViChn_Od, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VDA OD Start failed!\n");
        goto END0;
    }

    printf("Press any key to stop!");
    getchar();

    SAMPLE_COMM_VDA_OdStop(VdaChn_Od, ViChn_Od);
    SAMPLE_COMM_VDA_MdStop(VdaChn_Md, ViChn_Md);
    /******************************************
     step  4: stop vi vo & sys
    ******************************************/
    /******************************************
     step 8: exit process
    ******************************************/
    for(i=0;i<u32WndNum;i++)
    {
        VoChn = i;
        SAMPLE_COMM_VO_UnBindVi(VoDev,VoChn);
    }
    SAMPLE_COMM_VO_StopChn(VoDev, enVoMode);
    SAMPLE_COMM_VO_StopDevLayer(VoDev);
    SAMPLE_COMM_VI_Stop(enViMode);
END0:
    SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
