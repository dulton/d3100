/******************************************************************************
  A simple program of Hisilicon HI3531 video cascade implementation.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-8 Created
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h" 

VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;
HI_U32 gs_u32ViFrmRate = 0;

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_CAS_Slave_Usage(char *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0) Cascade slave chip's master mode,VI:16*D1; VO:single cascade output.\n");    
    printf("\t 1) Cascade slave chip's slave mode,VI:16*D1; VO:single cascade output.\n");
			
    return;
}

/******************************************************************************
* function : to process abnormal case                                         
******************************************************************************/
void SAMPLE_CAS_Slave_HandleSig(HI_S32 signo)
{    
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

/******************************************************************************
* function : cascade process
*            vo is hd : vi 16D1 -> vo cascade output
******************************************************************************/
HI_S32 SAMPLE_CAS_Slave_SingleProcess(HI_S32 s32StartChn,
    HI_S32 s32EndChn, SAMPLE_VO_MODE_E enVoMode, HI_BOOL bSlave)
{
    HI_S32 s32Ret, i;
    VB_CONF_S stVbConf;
    
    SIZE_S stSize;
    VPSS_GRP VpssGrp;
    
    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CAS_ATTR_S stCasAttr;
    HI_U32 u32WndNum, u32BlkSize, u32Pos, u32Pattern;
 
    /******************************************
     step 1: init varaible.
    ******************************************/
    switch (enVoMode)
    {
        case VO_MODE_1MUX:
            u32WndNum = 1;
            u32Pattern = 1;
            break;
        case VO_MODE_4MUX:
            u32WndNum = 4;
            u32Pattern = 4;
            break;
        case VO_MODE_9MUX:
            u32WndNum = 9;
            u32Pattern = 9;
            break;
        case VO_MODE_16MUX:
            u32WndNum = 16;
            u32Pattern = 16;
            break;
        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }
    
    gs_u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL== gs_enNorm)?25:30;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, PIC_D1, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }    
    
    /******************************************
     step 2: mpp system init.
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));
    stVbConf.u32MaxPoolCnt = 128;

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

    /*ddr0 video buffer*/
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 16 * 3;
    memset(stVbConf.astCommPool[0].acMmzName,0,
        sizeof(stVbConf.astCommPool[0].acMmzName));

    /*ddr1 video buffer*/
    stVbConf.astCommPool[1].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 16 * 3;
    strcpy(stVbConf.astCommPool[1].acMmzName,"ddr1");
    
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("mpp init failed!\n");
        return HI_FAILURE;
    }    

    s32Ret = SAMPLE_COMM_VI_MemConfig(SAMPLE_VI_MODE_16_D1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_MemConfig failed with %d!\n", s32Ret);
        goto END_0;
    }    
    
    s32Ret = SAMPLE_COMM_VPSS_MemConfig();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_MemConfig failed with %d!\n", s32Ret);
        goto END_0;
    }
    
    s32Ret = SAMPLE_COMM_VO_MemConfig(SAMPLE_VO_DEV_DHD0, NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_MemConfig failed with %d!\n", s32Ret);
        goto END_0;
    }
    
    /******************************************
     step 3: start vi.
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_Start(SAMPLE_VI_MODE_16_D1, gs_enNorm);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vi start failed!\n");
        goto END_1;
    }

    /******************************************
     step 4: start vpss, and bind vi to vpss.
    ******************************************/
    s32Ret = SAMPLE_COMM_VPSS_Start(16, &stSize, VPSS_MAX_CHN_NUM, NULL);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vpss start failed!\n");
        goto END_2;
    }

    s32Ret = SAMPLE_COMM_VI_BindVpss(SAMPLE_VI_MODE_16_D1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vi bind Vpss failed!\n");
        goto END_2;
    }
 
    /******************************************
     step 5: start vo, enable cascade
    ******************************************/
    stCasAttr.bSlave = bSlave;
    stCasAttr.enCasMode = VO_CAS_MODE_SINGLE;
    stCasAttr.enCasRgn = VO_CAS_64_RGN;

    /* set cascade attr */
    s32Ret = HI_MPI_VO_SetCascadeAttr(&stCasAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Set HI_MPI_VO_SetCascadeAttr failed!\n");
        goto END_3;
    }    
    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;    
    stVoPubAttr.enIntfType = VO_INTF_BT1120;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;

    /* start hd0 */
    s32Ret = HI_MPI_VO_SetPubAttr(SAMPLE_VO_DEV_DHD0, &stVoPubAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto END_3;
    }

    s32Ret = HI_MPI_VO_Enable(SAMPLE_VO_DEV_DHD0);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto END_3;
    }
    
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;
    stLayerAttr.u32DispFrmRt = 25;
    stLayerAttr.stDispRect.s32X       = 0;
    stLayerAttr.stDispRect.s32Y       = 0;
    stLayerAttr.stDispRect.u32Width   = 1920;
    stLayerAttr.stDispRect.u32Height  = 1080;
    stLayerAttr.stImageSize.u32Width  = 1920;
    stLayerAttr.stImageSize.u32Height = 1080;
    
    s32Ret = HI_MPI_VO_SetVideoLayerAttr(SAMPLE_VO_DEV_DHD0, &stLayerAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto END_3;
    }

    s32Ret = HI_MPI_VO_EnableVideoLayer(SAMPLE_VO_DEV_DHD0);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto END_3;
    }

    /* start cascade dev */
    s32Ret = HI_MPI_VO_EnableCascadeDev(VO_CAS_DEV_1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start VO_CAS_DEV_1 failed!\n");
        goto END_3;
    }

    /* start cascade dev's chn */
    s32Ret = SAMPLE_COMM_VO_StartChn(VO_CAS_DEV_1, &stVoPubAttr, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_3;
    }

    /* bind vpss to cascade dev's chn, bind cascade pos */
    for(i=0;i<u32WndNum;i++)
    {
        if (i<s32StartChn || i >s32EndChn)
        {
            HI_MPI_VO_DisableChn(VO_CAS_DEV_1, i);
            continue;
        }
        VoChn = i;
        
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VO_BindVpss(VO_CAS_DEV_1,VoChn,VpssGrp,VPSS_PRE0_CHN);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
            goto END_3;
        }

        u32Pos = i;
        s32Ret = HI_MPI_VO_CascadePosBindChn(u32Pos, VO_CAS_DEV_1, VoChn);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_CascadePosBindChn failed!\n");
            goto END_3;
        }
    }

    /* set pattern; start cascade output */
    s32Ret = HI_MPI_VO_SetCascadePattern(VO_CAS_DEV_1, u32Pattern);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VO_SetCascadePattern failed!\n");
        goto END_3;
    }

    s32Ret = HI_MPI_VO_EnableCascade();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VO_EnableCascade failed!\n");
        goto END_3;
    }
    
    printf("press two enter to quit!\n");
    getchar();
    getchar();
    /******************************************
     step 6: disable cascade, unbind vo and vpss
    ******************************************/
END_3:
    /* stop cascade */
    HI_MPI_VO_DisableCascade();

    /* unbind pos, unbind vo and vpss */
    for(i=0;i<u32WndNum;i++)
    {
        if (i<s32StartChn || i >s32EndChn)
        {
            continue;
        }
        VoChn = i;        
        VpssGrp = i;
        u32Pos = i;
        HI_MPI_VO_CascadePosUnBindChn(u32Pos, VO_CAS_DEV_1, VoChn);
        SAMPLE_COMM_VO_UnBindVpss(VO_CAS_DEV_1, VoChn, VpssGrp, VPSS_PRE0_CHN);
    }

    /* stop cascade dev's chn */
    SAMPLE_COMM_VO_StopChn(VO_CAS_DEV_1, enVoMode);

    /* stop cascade dev */
    HI_MPI_VO_DisableCascadeDev(VO_CAS_DEV_1);

    /* disable hd0 */
    SAMPLE_COMM_VO_StopDevLayer(SAMPLE_VO_DEV_DHD0);
    /******************************************
     step 7: unbind vi and vpss, stop vpss
    ******************************************/
END_2:
    /* unbind vi and vpss */
    SAMPLE_COMM_VI_UnBindVpss(SAMPLE_VI_MODE_16_D1);

    /* stop vpss */
    SAMPLE_COMM_VPSS_Stop(16, VPSS_MAX_CHN_NUM);
    /******************************************
     step 8: stop vi
    ******************************************/
END_1:
    SAMPLE_COMM_VI_Stop(SAMPLE_VI_MODE_16_D1);
    /******************************************
     step 9: exit mpp system
    ******************************************/
END_0:
    SAMPLE_COMM_SYS_Exit();

    return HI_SUCCESS;
}

/****************************************************************************
* function: main
****************************************************************************/
int main(int argc, char* argv[])
{
    HI_S32 s32Index;
    HI_S32 s32StartChn, s32EndChn;

    if (argc != 2)
    {
        SAMPLE_CAS_Slave_Usage(argv[0]);
        return HI_FAILURE;
    }
    
    signal(SIGINT, SAMPLE_CAS_Slave_HandleSig);
    signal(SIGTERM, SAMPLE_CAS_Slave_HandleSig);

    s32Index = atoi(argv[1]);

    switch (s32Index)
    {
        case 0:
            gs_enNorm = VIDEO_ENCODING_MODE_NTSC;
            s32StartChn = 0;
            s32EndChn = 1;
            SAMPLE_CAS_Slave_SingleProcess(s32StartChn, s32EndChn, VO_MODE_4MUX, HI_FALSE);
            break;
        case 1:
            gs_enNorm = VIDEO_ENCODING_MODE_NTSC;
            s32StartChn = 2;
            s32EndChn = 2;
            SAMPLE_CAS_Slave_SingleProcess(s32StartChn, s32EndChn, VO_MODE_4MUX, HI_TRUE);
            break;
        default:
            printf("the index is invaild!\n");
            SAMPLE_CAS_Slave_Usage(argv[0]);
            return HI_FAILURE;
        break;
    }

    return HI_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



