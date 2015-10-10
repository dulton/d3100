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

#define SAMPLE_MAX_VDEC_CHN_CNT 32
typedef struct sample_vdec_sendparam
{
    pthread_t Pid;
    HI_BOOL bRun;
    VDEC_CHN VdChn;    
    PAYLOAD_TYPE_E enPayload;
	HI_S32 s32MinBufSize;
    VIDEO_MODE_E enVideoMode;
}SAMPLE_VDEC_SENDPARAM_S;

VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;
HI_U32 gs_u32ViFrmRate = 0;
SAMPLE_VDEC_SENDPARAM_S gs_SendParam[SAMPLE_MAX_VDEC_CHN_CNT];
HI_S32 gs_s32VdecCnt;
/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_CAS_Master_Usage(char *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0) Cascade master chip,VI:cascade input; VDEC -> VO; VO:hdmi output.\n");
    return;
}

/******************************************************************************
* function : to process abnormal case                                         
******************************************************************************/
void SAMPLE_CAS_Master_HandleSig(HI_S32 signo)
{
    HI_S32 i;
    
    if (SIGINT == signo || SIGTSTP == signo)
    {
        for (i=0; i<gs_s32VdecCnt; i++)
        {
            gs_SendParam[i].bRun = HI_FALSE;
            pthread_join(gs_SendParam[i].Pid, 0);
            printf("join thread %d.\n", i);
        }
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}
    
/******************************************************************************
* function : send stream to vdec
******************************************************************************/
void* SAMPLE_CAS_Master_SendStream(void* p)
{
    VDEC_STREAM_S stStream;
    SAMPLE_VDEC_SENDPARAM_S *pstSendParam;
    char sFileName[50], sFilePostfix[20];
    FILE* fp = NULL;
    HI_S32 s32Ret;
    HI_S32 s32BlockMode = HI_IO_BLOCK;
    struct timeval stTime,*ptv; 
    HI_U8 *pu8Buf;
    HI_S32 s32LeftBytes,i;
    HI_BOOL bTimeFlag=HI_TRUE;
    HI_U64 pts= 1000;
    HI_S32 s32IntervalTime = 40000;
    
    HI_U32 u32StartCode[4] = {0x41010000, 0x67010000, 0x01010000, 0x61010000};
    HI_U16 u16JpegStartCode = 0xD9FF;

    s32LeftBytes = 0;

    pstSendParam = (SAMPLE_VDEC_SENDPARAM_S *)p;

    /*open the stream file*/
    SAMPLE_COMM_SYS_Payload2FilePostfix(pstSendParam->enPayload, sFilePostfix);
    sprintf(sFileName, "stream_chn%d%s", (HI_S32)pstSendParam->VdChn, sFilePostfix);
    fp = fopen(sFileName, "r");
    if (HI_NULL == fp)
    {
        SAMPLE_PRT("open file %s err\n", sFileName);
        return NULL;
    }
    printf("open file [%s] ok!\n", sFileName);

    if(pstSendParam->s32MinBufSize!=0)
    {
        pu8Buf=malloc(pstSendParam->s32MinBufSize);
        if(pu8Buf==NULL)
        {
            SAMPLE_PRT("can't alloc %d in send stream thread:%d\n",pstSendParam->s32MinBufSize,pstSendParam->VdChn);
            fclose(fp);
            return (HI_VOID *)(HI_FAILURE);
        }
    }
    else
    {
    	SAMPLE_PRT("none buffer to operate in send stream thread:%d\n",pstSendParam->VdChn);
    	return (HI_VOID *)(HI_FAILURE);
    }
    ptv=(struct timeval *)&stStream.u64PTS;

    while (pstSendParam->bRun)
    {
        if(gettimeofday(&stTime,NULL))
        {
            if(bTimeFlag)
                SAMPLE_PRT("can't get time for pts in send stream thread %d\n",pstSendParam->VdChn);
            bTimeFlag=HI_FALSE;
        }
        stStream.u64PTS= 0;//((HI_U64)(stTime.tv_sec)<<32)|((HI_U64)stTime.tv_usec);
        stStream.pu8Addr=pu8Buf;
        stStream.u32Len=fread(pu8Buf+s32LeftBytes,1,pstSendParam->s32MinBufSize-s32LeftBytes,fp);
        // SAMPLE_PRT("bufsize:%d,readlen:%d,left:%d\n",pstVdecThreadParam->s32MinBufSize,stStream.u32Len,s32LeftBytes);
        s32LeftBytes=stStream.u32Len+s32LeftBytes;
       
        if((pstSendParam->enVideoMode==VIDEO_MODE_FRAME)&&(pstSendParam->enPayload== PT_H264))
        {
            HI_U8 *pFramePtr;
            HI_U32 u32StreamVal;
            HI_BOOL bFindStartCode = HI_FALSE;
            pFramePtr=pu8Buf+4;
            for(i=0;i<(s32LeftBytes-4);i++)
            {
                u32StreamVal=(pFramePtr[0]);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[1]<<8);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[2]<<16);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[3]<<24);
                if(  (u32StreamVal==u32StartCode[1])||
                (u32StreamVal==u32StartCode[0])||
                (u32StreamVal==u32StartCode[2])||
                (u32StreamVal==u32StartCode[3]))
            	 {
                    bFindStartCode = HI_TRUE;
                    break;
            	 }
            	pFramePtr++;
            }
            if (HI_FALSE == bFindStartCode)
            {
                printf("\033[0;31mALERT!!!,the search buffer is not big enough for one frame!!!%d\033[0;39m\n",
                pstSendParam->VdChn);
            }
        	i=i+4;
        	stStream.u32Len=i;
        	s32LeftBytes=s32LeftBytes-i;
        }
        else if((pstSendParam->enVideoMode==VIDEO_MODE_FRAME)&&((pstSendParam->enPayload== PT_JPEG)
            ||(pstSendParam->enPayload == PT_MJPEG)))
        {
            HI_U8 *pFramePtr;
            HI_U16 u16StreamVal;
            HI_BOOL bFindStartCode = HI_FALSE;
            pFramePtr=pu8Buf; 
            for(i=0;i<(s32LeftBytes-1);i++)
            {
                u16StreamVal=(pFramePtr[0]);
                u16StreamVal=u16StreamVal|((HI_U16)pFramePtr[1]<<8);
                if(  (u16StreamVal == u16JpegStartCode))
                {
                    bFindStartCode = HI_TRUE;
                    break;
                }
                pFramePtr++;
            }
            if (HI_FALSE == bFindStartCode)
            {
                printf("\033[0;31mALERT!!!,the search buffer is not big enough for one frame!!!%d\033[0;39m\n",
                pstSendParam->VdChn);
            }
            i=i+2;
            stStream.u32Len=i;
            s32LeftBytes=s32LeftBytes-i;
        }
        else // stream mode 
        {
            stStream.u32Len=s32LeftBytes;
            s32LeftBytes=0;
        }

        pts+=40000;
        stStream.u64PTS = pts;
        s32Ret=HI_MPI_VDEC_SendStream(pstSendParam->VdChn, &stStream, s32BlockMode);
        if (HI_SUCCESS != s32Ret)
        {
            //SAMPLE_PRT("failret:%x\n",s32Ret);
            sleep(s32IntervalTime);
        }
        if(s32BlockMode==HI_IO_NOBLOCK && s32Ret==HI_FAILURE)
        {
            sleep(s32IntervalTime);
        }
        else if(s32BlockMode==HI_IO_BLOCK && s32Ret==HI_FAILURE)
        {
            SAMPLE_PRT("can't send stream in send stream thread %d\n",pstSendParam->VdChn);
            sleep(s32IntervalTime);
        }
        if(pstSendParam->enVideoMode==VIDEO_MODE_FRAME && s32Ret==HI_SUCCESS)
        {
            memcpy(pu8Buf,pu8Buf+stStream.u32Len,s32LeftBytes);
        }
        else if (pstSendParam->enVideoMode==VIDEO_MODE_FRAME && s32Ret!=HI_SUCCESS)
        {
            s32LeftBytes = s32LeftBytes+stStream.u32Len;
        }

        if(stStream.u32Len!=(pstSendParam->s32MinBufSize-s32LeftBytes))
        {
            printf("file end.\n");
            //fseek(fp,0,SEEK_SET);
            break;
        }

        usleep(20000);
    }
    fflush(stdout);
    free(pu8Buf);
    fclose(fp);

    return (HI_VOID *)HI_SUCCESS;
}

/******************************************************************************
* function : create vdec chn
******************************************************************************/
static HI_S32 SAMPLE_CAS_Master_CreateVdecChn(HI_S32 s32ChnID, SIZE_S *pstSize, PAYLOAD_TYPE_E enType, VIDEO_MODE_E enVdecMode)
{
    VDEC_CHN_ATTR_S stAttr;
    HI_S32 s32Ret;

    memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));

    stAttr.enType = enType;
    stAttr.u32BufSize = pstSize->u32Height * pstSize->u32Width;//This item should larger than u32Width*u32Height/2
    stAttr.u32Priority = 1;//此处必须大于0
    stAttr.u32PicWidth = pstSize->u32Width;
    stAttr.u32PicHeight = pstSize->u32Height;
    
    switch (enType)
    {
        case PT_H264:
	    stAttr.stVdecVideoAttr.u32RefFrameNum = 1;
	    stAttr.stVdecVideoAttr.enMode = enVdecMode;
	    stAttr.stVdecVideoAttr.s32SupportBFrame = 1;
            break;
        case PT_JPEG:
            stAttr.stVdecJpegAttr.enMode = enVdecMode;
            break;
        case PT_MJPEG:
            stAttr.stVdecJpegAttr.enMode = enVdecMode;
            break;
        default:
            SAMPLE_PRT("err type \n");
            return HI_FAILURE;
    }

    s32Ret = HI_MPI_VDEC_CreateChn(s32ChnID, &stAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_CreateChn failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VDEC_StartRecvStream(s32ChnID);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_StartRecvStream failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

/******************************************************************************
* function : force to stop decoder and destroy channel.
*            stream left in decoder will not be decoded.
******************************************************************************/
void SAMPLE_CAS_Master_ForceDestroyVdecChn(HI_S32 s32ChnID)
{
    HI_S32 s32Ret;

    s32Ret = HI_MPI_VDEC_StopRecvStream(s32ChnID);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_StopRecvStream failed errno 0x%x \n", s32Ret);
    }

    s32Ret = HI_MPI_VDEC_DestroyChn(s32ChnID);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_DestroyChn failed errno 0x%x \n", s32Ret);
    }
}

/******************************************************************************
* function : wait for decoder finished and destroy channel.
*            Stream left in decoder will be decoded.
******************************************************************************/
void SAMPLE_CAS_Master_WaitDestroyVdecChn(HI_S32 s32ChnID, VIDEO_MODE_E enVdecMode)
{
    HI_S32 s32Ret;
    VDEC_CHN_STAT_S stStat;

    memset(&stStat, 0, sizeof(VDEC_CHN_STAT_S));

    s32Ret = HI_MPI_VDEC_StopRecvStream(s32ChnID);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VDEC_StopRecvStream failed errno 0x%x \n", s32Ret);
        return;
    }

    /*** wait destory ONLY used at frame mode! ***/
    if (VIDEO_MODE_FRAME == enVdecMode)
    {
        while (1)
        {
            usleep(40000);
            s32Ret = HI_MPI_VDEC_Query(s32ChnID, &stStat);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VDEC_Query failed errno 0x%x \n", s32Ret);
                return;
            }
            if ((stStat.u32LeftPics == 0) && (stStat.u32LeftStreamFrames == 0))
            {
                printf("had no stream and pic left\n");
                break;
            }
        }
    }
    s32Ret = HI_MPI_VDEC_DestroyChn(s32ChnID);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VDEC_DestroyChn failed errno 0x%x \n", s32Ret);
        return;
    }
}

/******************************************************************************
* function : cascade process
*            vo is hd : vi cas chn -> vo
*                       vdec -> vpss -> vo
******************************************************************************/
HI_S32 SAMPLE_CAS_Master_SingleProcess(PIC_SIZE_E enVdecPicSize, HI_S32 s32VdecCnt,
    VO_DEV VoDev, SAMPLE_VO_MODE_E enVoMode)
{
    VDEC_CHN VdChn;
    HI_S32 s32Ret;
    SIZE_S stSize;
    VB_CONF_S stVbConf;
    HI_S32 i;
    VPSS_GRP VpssGrp;
    VIDEO_MODE_E enVdecMode;

    VI_DEV ViDev;
    VI_CHN ViChn;
    SIZE_S stDestSize;
    RECT_S stCapRect;

    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_ZOOM_ATTR_S stZoomAttr;
    VO_CHN_ATTR_S stChnAttr;
    HI_U32 u32WndNum, u32BlkSize;
 
    /******************************************
     step 1: init varaible.
    ******************************************/
    switch (enVoMode)
    {
        case VO_MODE_1MUX:
            u32WndNum = 1;
            break;
        case VO_MODE_4MUX:
            u32WndNum = 4;
            break;
        case VO_MODE_9MUX:
            u32WndNum = 9;
            break;
        case VO_MODE_16MUX:
            u32WndNum = 16;
            break;
        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }
    
    if (s32VdecCnt > SAMPLE_MAX_VDEC_CHN_CNT)
    {
        SAMPLE_PRT("Vdec count is bigger than sample define!\n");
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enVdecPicSize, &stSize);
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
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

    /*ddr0 video buffer*/
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 3;
    memset(stVbConf.astCommPool[0].acMmzName,0,
        sizeof(stVbConf.astCommPool[0].acMmzName));

    /*ddr1 video buffer*/
    stVbConf.astCommPool[1].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 3;
    strcpy(stVbConf.astCommPool[1].acMmzName,"ddr1");

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

    /*ddr0 video buffer*/
    stVbConf.astCommPool[2].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[2].u32BlkCnt = 3;
    memset(stVbConf.astCommPool[2].acMmzName,0,
        sizeof(stVbConf.astCommPool[2].acMmzName));

    /*ddr1 video buffer*/
    stVbConf.astCommPool[3].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[3].u32BlkCnt = 3;
    strcpy(stVbConf.astCommPool[3].acMmzName,"ddr1");

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("mpp init failed!\n");
        return HI_FAILURE;
    }    

    s32Ret = SAMPLE_COMM_VI_MemConfig(SAMPLE_VI_MODE_4_1080P);
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
        
    s32Ret = SAMPLE_COMM_VDEC_MemConfig();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_MemConfig failed with %d!\n", s32Ret);
        goto END_0;
    }
    
    s32Ret = SAMPLE_COMM_VO_MemConfig(VoDev, NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_MemConfig failed with %d!\n", s32Ret);
        goto END_0;
    }
    
    /******************************************
     step 3: start vpss for vdec.
    ******************************************/    
    s32Ret = SAMPLE_COMM_VPSS_Start(s32VdecCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vpss start failed!\n");
        goto END_0;
    }
 
    /******************************************
     step 4: start vo
    ******************************************/    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;    
    stVoPubAttr.enIntfType = VO_INTF_HDMI;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;
    
    s32Ret = SAMPLE_COMM_VO_StartDevLayer(VoDev, &stVoPubAttr, 60);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartDevLayer failed!\n");
        goto END_1;
    }
    
    s32Ret = SAMPLE_COMM_VO_StartChn(VoDev, &stVoPubAttr, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_2;
    }

    /* if it's displayed on HDMI, we should start HDMI */
    if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
    {
        if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
        {
            SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
            goto END_2;
        }
    }    

    for(i=0;i<s32VdecCnt;i++)
    {
        VoChn = (u32WndNum - s32VdecCnt) + i;
        
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VO_BindVpss(VoDev,VoChn,VpssGrp,VPSS_PRE0_CHN);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
            goto END_2;
        }
    }

    /******************************************
     step 5: start vdec & bind it to vpss
    ******************************************/
    /*** set vdec frame mode ***/
    enVdecMode = VIDEO_MODE_FRAME;
    
    for (i=0; i<s32VdecCnt; i++)
    {
        /*** create vdec chn ***/
        VdChn = i;
        s32Ret = SAMPLE_CAS_Master_CreateVdecChn(VdChn, &stSize, PT_H264, enVdecMode);
        if (HI_SUCCESS !=s32Ret)
        {
            SAMPLE_PRT("create vdec chn failed!\n");
            goto END_3;
        }
        /*** bind vdec to vpss ***/        
        VpssGrp = i;
        s32Ret = SAMLE_COMM_VDEC_BindVpss(VdChn, VpssGrp);
        if (HI_SUCCESS !=s32Ret)
        {
            SAMPLE_PRT("vdec(vdch=%d) bind vpss(vpssg=%d) failed!\n", VdChn, VpssGrp);
            goto END_3;
        }
    }

    /******************************************
     step 6: open file & video decoder
    ******************************************/
    for (i=0; i<s32VdecCnt; i++)
    {
        gs_SendParam[i].bRun = HI_TRUE;
        gs_SendParam[i].VdChn = i;
        gs_SendParam[i].enPayload = PT_H264;
        gs_SendParam[i].enVideoMode = enVdecMode;
        gs_SendParam[i].s32MinBufSize = stSize.u32Height * stSize.u32Width / 2;
        pthread_create(&gs_SendParam[i].Pid, NULL, SAMPLE_CAS_Master_SendStream, &gs_SendParam[i]);
    }

    /******************************************
     step 7: start vi cascade
    ******************************************/
    ViDev = 2;
    ViChn = 4;
    s32Ret = SAMPLE_COMM_VI_Mode2Size(SAMPLE_VI_MODE_4_1080P, gs_enNorm, &stCapRect, &stDestSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vi get size failed!\n");
        goto END_4;
    }
    
    s32Ret = SAMPLE_COMM_VI_StartDev(ViDev, SAMPLE_VI_MODE_4_1080P);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartDev failed with %#x\n", s32Ret);
        goto END_4;
    }

    s32Ret = SAMPLE_COMM_VI_StartChn(ViChn, &stCapRect, &stDestSize, SAMPLE_VI_MODE_4_1080P, VI_CHN_SET_NORMAL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("call SAMPLE_COMM_VI_StarChn failed with %#x\n", s32Ret);
        goto END_4;
    }

    s32Ret = HI_MPI_VI_EnableCascade(ViDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("call HI_MPI_VI_EnableCascade failed with %#x\n", s32Ret);
        goto END_4;
    }

    s32Ret = HI_MPI_VI_EnableCascadeChn(VI_CAS_CHN_1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("call HI_MPI_VI_EnableCascadeChn failed with %#x\n", s32Ret);
        goto END_4;
    }

    /******************************************
     step 8: bind vi&vo, set vo zoom in
    ******************************************/    
    stZoomAttr.enZoomType = VOU_ZOOM_IN_RECT;
    for (i=0; i<u32WndNum - s32VdecCnt; i++)
    {
        /*** bind vi to vo ***/        
        s32Ret = SAMPLE_COMM_VO_BindVi(VoDev, i, VI_CAS_CHN_1);
        if (HI_SUCCESS !=s32Ret)
        {
            SAMPLE_PRT("vi(vichn=%d) bind vo(vodev=%d,vochn=%d) failed!\n", ViChn, VoDev, i);
            goto END_5;
        }
        
        HI_MPI_VO_GetChnAttr(VoDev, i, &stChnAttr);
        memcpy(&stZoomAttr.stZoomRect, &stChnAttr.stRect, sizeof(RECT_S));
        s32Ret = HI_MPI_VO_SetZoomInWindow(VoDev, i, &stZoomAttr);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("call HI_MPI_VO_SetZoomInWindow failed with %#x\n", s32Ret);
            goto END_5;
        }
    }
    
    printf("press two enter to quit!\n");
    getchar();
    getchar();
    /******************************************
     step 9: unbind vi&vo
    ******************************************/
END_5:
    for (i=0; i<u32WndNum - s32VdecCnt; i++)
    {
        /*** unbind vi to vo ***/        
        s32Ret = SAMPLE_COMM_VO_UnBindVi(VoDev, i);
    }
    /******************************************
     step 10: stop vi cascade
    ******************************************/
END_4:
    HI_MPI_VI_DisableCascadeChn(VI_CAS_CHN_1);
    SAMPLE_COMM_VI_Stop(SAMPLE_VI_MODE_4_1080P);
    HI_MPI_VI_DisableCascade(ViDev);
    /******************************************
     step 11: join thread
    ******************************************/
    for (i=0; i<s32VdecCnt; i++)
    {
        gs_SendParam[i].bRun = HI_FALSE;
        pthread_join(gs_SendParam[i].Pid, 0);
        printf("join thread %d.\n", i);
    }

    /******************************************
     step 12: Unbind vdec to vpss & destroy vdec-chn
    ******************************************/
END_3:
    for (i=0; i<s32VdecCnt; i++)
    {
        VdChn = i;
        SAMPLE_CAS_Master_WaitDestroyVdecChn(VdChn, enVdecMode);
        VpssGrp = i;
        SAMLE_COMM_VDEC_UnBindVpss(VdChn, VpssGrp);
    }
    /******************************************
     step 13: stop vo
    ******************************************/
END_2:
    for(i=0;i<s32VdecCnt;i++)
    {
        VoChn = (u32WndNum - s32VdecCnt) + i;        
        VpssGrp = i;
        SAMPLE_COMM_VO_UnBindVpss(VoDev,VoChn,VpssGrp,VPSS_PRE0_CHN);
    }
    SAMPLE_COMM_VO_StopChn(VoDev, enVoMode);
    SAMPLE_COMM_VO_StopDevLayer(VoDev);
    if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
    {
        SAMPLE_COMM_VO_HdmiStop();
    }
END_1:    
    SAMPLE_COMM_VPSS_Stop(s32VdecCnt, VPSS_MAX_CHN_NUM);
    /******************************************
     step 14: exit mpp system
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

#if HICHIP == HI3532_V100 
    printf("Hi3532 not support this sample(cascade master), program will exit directly.\n");
    return HI_SUCCESS;
#endif

    if (argc != 2)
    {
        SAMPLE_CAS_Master_Usage(argv[0]);
        return HI_FAILURE;
    }
    
    signal(SIGINT, SAMPLE_CAS_Master_HandleSig);
    signal(SIGTERM, SAMPLE_CAS_Master_HandleSig);

    s32Index = atoi(argv[1]);

    switch (s32Index)
    {
        case 0:
            gs_s32VdecCnt = 1;
            gs_enNorm = VIDEO_ENCODING_MODE_PAL;
            SAMPLE_CAS_Master_SingleProcess(PIC_D1, gs_s32VdecCnt, SAMPLE_VO_DEV_DHD0, VO_MODE_4MUX);
            break;
        default:
            printf("the index is invaild!\n");
            SAMPLE_CAS_Master_Usage(argv[0]);
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


