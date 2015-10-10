/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_pciv_host.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/09/22
  Description   : this sample of pciv in PCI host
  History       :
  1.Date        : 2009/09/22
    Author      : Hi3520MPP
    Modification: Created file
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: 将消息端口的打开操作放到最开始的初始化过程中
  3.Date        : 2010/06/10
    Author      : Hi3520MPP
    Modification: 调整启动PCIV通道的相关函数封装；并支持同一输入画面输出到主片的多个VO设备上显示
******************************************************************************/

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
#include <math.h>

#include "hi_comm_pciv.h"
#include "mpi_pciv.h"
#include "mpi_vdec.h"
#include "mpi_vpss.h"
#include "pciv_msg.h"
#include "pciv_trans.h"
#include "sample_pciv_comm.h"
#include "hi_debug.h"

#include "sample_comm.h"
#include "sample_common.h"

typedef struct hiSAMPLE_PCIV_DISP_CTX_S
{
    VO_DEV VoDev;       /* VO 设备号 */
    HI_BOOL bValid;     /* 是否使用此显示设备*/
    HI_U32 u32PicDiv;   /* 当前显示设备的画面分割数 */
    VO_CHN VoChnStart;  /* 当前显示设备起始VO通道号 */
    VO_CHN VoChnEnd;    /* 当前显示设备结束VO通道号 */
} SAMPLE_PCIV_DISP_CTX_S;

extern VIDEO_NORM_E   gs_enViNorm;
extern VO_INTF_SYNC_E gs_enSDTvMode;
extern VI_DEV_ATTR_S DEV_ATTR_BT656D1_4MUX;
extern VI_DEV_ATTR_S DEV_ATTR_BT656D1_1MUX;

#if 1
#define SAMPLE_PCIV_VDEC_SIZE PIC_CIF
#define SAMPLE_PCIV_VDEC_FILE "sample_cif_25fps.h264"
#else
#define SAMPLE_PCIV_VDEC_SIZE PIC_D1
#define SAMPLE_PCIV_VDEC_FILE "sample_d1.h264"
#endif

/* max pciv chn count in one vo dev */
#define SAMPLE_PCIV_CNT_PER_VO      16
#define PCIV_FRMNUM_ONCEDMA 5

/* max pciv chn count in one pci dev */
#define SAMPLE_PCIV_CNT_PER_DEV     SAMPLE_PCIV_CNT_PER_VO * VO_MAX_DEV_NUM

static HI_U32 g_u32PfWinBase[PCIV_MAX_CHIPNUM]  = {0};
VIDEO_NORM_E   g_enViNorm   = VIDEO_ENCODING_MODE_PAL;

static SAMPLE_PCIV_VENC_CTX_S g_stPcivVencCtx = {0};
static SAMPLE_PCIV_VDEC_CTX_S astPcivVdecCtx[VDEC_MAX_CHN_NUM];
static SAMPLE_PCIV_DISP_CTX_S s_astPcivDisp[VO_MAX_DEV_NUM] =
{
    {.bValid = HI_TRUE,   .VoDev =  2, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 15},
    {.bValid = HI_FALSE,  .VoDev =  1, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 15},
    {.bValid = HI_FALSE,  .VoDev =  0, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 1}
};
static PCIV_BIND_OBJ_S g_stLocBind[PCIV_MAX_CHN_NUM], g_stRmtBind[PCIV_MAX_CHN_NUM];

#define STREAM_SEND_VDEC    1
#define STREAM_SAVE_FILE    0
#define PCIV_START_STREAM   1
//#define TEST_OTHER_DIVISION 1
//#define HOST_PCIV_BIND_VENC 1

static int test_idx = 0;
static int vdec_idx = 0;
static int Add_osd  = 0;


HI_S32 SAMPLE_PCIV_SaveFile(PCIV_CHN PcivChn);

static HI_VOID TraceStreamInfo(VENC_CHN vencChn, VENC_STREAM_S *pstVStream)
{
    if ((pstVStream->u32Seq % SAMPLE_PCIV_SEQ_DEBUG) == 0 )
    {
        printf("Send VeStream -> VeChn[%2d], PackCnt[%d], Seq:%d, DataLen[%6d], hours:%d\n",
            vencChn, pstVStream->u32PackCount, pstVStream->u32Seq,
            pstVStream->pstPack[0].u32Len[0],
            pstVStream->u32Seq/(25*60*60));
    }
}


HI_S32 SamplePcivGetPicSize(HI_S32 s32VoDiv, SIZE_S *pstPicSize)
{
    SIZE_S stScreemSize;

    stScreemSize.u32Width  = 720;
    stScreemSize.u32Height = (VIDEO_ENCODING_MODE_PAL==g_enViNorm) ? 576 : 480;

    switch (s32VoDiv)
    {
        case 1 :
            pstPicSize->u32Width  = stScreemSize.u32Width;
            pstPicSize->u32Height = stScreemSize.u32Height;
            break;
        case 4 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 2;
            pstPicSize->u32Height = stScreemSize.u32Height / 2;
            break;
        case 9 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 3;
            pstPicSize->u32Height = stScreemSize.u32Height / 3;
            break;
        case 16 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 4;
            pstPicSize->u32Height = stScreemSize.u32Height / 4;
            break;
#if TEST_OTHER_DIVISION
        case 76 :
            pstPicSize->u32Width  = 76;
            pstPicSize->u32Height = 76;
            break;
        case 100 :
            pstPicSize->u32Width  = 100;
            pstPicSize->u32Height = 100;
            break;
        case 114 :
            pstPicSize->u32Width  = 114;
            pstPicSize->u32Height = 114;
            break;    
        case 132 :
            pstPicSize->u32Width  = 132;
            pstPicSize->u32Height = 132;
            break;   
        case 174 :
            pstPicSize->u32Width  = 174;
            pstPicSize->u32Height = 174;
            break;       
        case 200 :
            pstPicSize->u32Width  = 200;
            pstPicSize->u32Height = 200;
            break;
        case 242 :
            pstPicSize->u32Width  = 242;
            pstPicSize->u32Height = 242;
            break;
        case 300 :
            pstPicSize->u32Width  = 300;
            pstPicSize->u32Height = 300;
            break;
        case 320 :
            pstPicSize->u32Width  = 320;
            pstPicSize->u32Height = 240;
            break;
        case 640 :
            pstPicSize->u32Width  = 640;
            pstPicSize->u32Height = 480;
            break;    
#endif
        default:
            printf("not support this vo div %d \n", s32VoDiv);
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePcivGetPicAttr(SIZE_S *pstPicSize, PCIV_PIC_ATTR_S *pstPicAttr)
{
    pstPicAttr->u32Width  = pstPicSize->u32Width;
    pstPicAttr->u32Height = pstPicSize->u32Height;
    pstPicAttr->u32Stride[0] = SAMPLE_PCIV_GET_STRIDE(pstPicAttr->u32Width, 16);
    pstPicAttr->u32Stride[1] = pstPicAttr->u32Stride[2] = pstPicAttr->u32Stride[0];

    pstPicAttr->u32Field  = VIDEO_FIELD_FRAME;
    pstPicAttr->enPixelFormat = SAMPLE_PIXEL_FORMAT;

    return HI_SUCCESS;
}

HI_S32 SamplePcivSendMsgDestroy(HI_S32 s32TargetId,PCIV_PCIVCMD_DESTROY_S *pstMsgDestroy)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    stMsg.stMsgHead.u32Target = 1;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_DESTROY_PCIV;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_DESTROY_S);
    memcpy(stMsg.cMsgBody, pstMsgDestroy, sizeof(PCIV_PCIVCMD_DESTROY_S));
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_DESTROY_PCIV==========\n");
    s32Ret = PCIV_SendMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePcivSendMsgCreate(HI_S32 s32TargetId,PCIV_PCIVCMD_CREATE_S *pstMsgCreate)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    stMsg.stMsgHead.u32Target = s32TargetId;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_CREATE_PCIV;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_CREATE_S);
    memcpy(stMsg.cMsgBody, pstMsgCreate, sizeof(PCIV_PCIVCMD_CREATE_S));
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_CREATE_PCIV==========\n");
    s32Ret = PCIV_SendMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);
    return HI_SUCCESS;
}

HI_VOID *SamplePciv_RevVeStrmThread(HI_VOID *arg)
{
    SAMPLE_PCIV_VENC_CTX_S *pstCtx = (SAMPLE_PCIV_VENC_CTX_S*)arg;
    HI_VOID *pReceiver = pstCtx->pTransHandle;
    PCIV_STREAM_HEAD_S *pStrmHead = NULL;
    HI_U8 *pu8Addr, *pu8AddrTmp;
    HI_U32 u32Len;
    HI_S32 s32VencChn;
#if STREAM_SAVE_FILE
    HI_CHAR aszFileName[64] = {0};
    HI_S32 s32WriteLen = 0;
    static FILE *pFile[VENC_MAX_CHN_NUM] = {NULL};
#endif

    /* 使操作系统支持非4字节对齐内存操作 */
    system("echo 2 > /proc/cpu/alignment");

    while (pstCtx->bThreadStart)
    {
        /* get data from pciv stream receiver */
        if (PCIV_Trans_GetData(pReceiver, &pu8Addr, &u32Len))
        {
            usleep(0);
            continue;
        }

        for (pu8AddrTmp = pu8Addr; pu8AddrTmp < (pu8Addr + u32Len); )
        {
            pStrmHead = (PCIV_STREAM_HEAD_S *)pu8AddrTmp;
            
            HI_ASSERT(PCIV_STREAM_MAGIC == pStrmHead->u32Magic);

            s32VencChn = pStrmHead->s32ChnID;
            HI_ASSERT(s32VencChn < VENC_MAX_CHN_NUM);

#if STREAM_SAVE_FILE
            /* save stream data to file */
            if (NULL == pFile[s32VencChn])
            {
                sprintf(aszFileName, "host_venc_chn%d.h264", s32VencChn);
                pFile[s32VencChn] = fopen(aszFileName, "wb");
                HI_ASSERT(pFile[s32VencChn]);
            }
            s32WriteLen = fwrite(pu8AddrTmp + sizeof(PCIV_STREAM_HEAD_S), pStrmHead->u32StreamDataLen, 1, pFile[s32VencChn]);
            HI_ASSERT(1 == s32WriteLen);
#endif
#if STREAM_SEND_VDEC
            /*send stream to vdec*/
            {
                HI_S32 s32Ret;
                VDEC_CHN VdChn = pStrmHead->s32ChnID;
                VDEC_STREAM_S stStream;
                stStream.pu8Addr = pu8AddrTmp + sizeof(PCIV_STREAM_HEAD_S);
                stStream.u32Len = pStrmHead->u32StreamDataLen;
                stStream.u64PTS = pStrmHead->u64PTS;
                do {
                    s32Ret = HI_MPI_VDEC_SendStream(VdChn, &stStream, HI_IO_NOBLOCK);
                    if (HI_FALSE == pstCtx->bThreadStart)
                        break;
                } while (s32Ret);
            }
#endif
            pu8AddrTmp += (sizeof(PCIV_STREAM_HEAD_S) + pStrmHead->u32DMADataLen);
            HI_ASSERT(pu8AddrTmp <= (pu8Addr + u32Len));
        }

        /* release data to pciv stream receiver */

        PCIV_Trans_ReleaseData(pReceiver, pu8Addr, u32Len);
        pstCtx->u32Seq ++;
    }

    pstCtx->bThreadStart = HI_FALSE;
    return NULL;
}

/* read stream data from file, write to local buffer, then send to pci target direct at once */
HI_VOID * SamplePciv_SendVdStrmThread(HI_VOID *p)
{
    HI_S32          s32Ret, s32ReadLen;
    HI_U32          u32FrmSeq = 0;
    HI_U8           *pu8VdecBuf = NULL;
	HI_VOID         *pCreator    = NULL;
    FILE* file = NULL;
    PCIV_STREAM_HEAD_S      stHeadTmp;
    PCIV_TRANS_LOCBUF_STAT_S stLocBufSta;
    SAMPLE_PCIV_VDEC_CTX_S *pstCtx = (SAMPLE_PCIV_VDEC_CTX_S*)p;

    pCreator = pstCtx->pTransHandle;
    printf("%s -> Sender:%p, chnid:%d\n", __FUNCTION__, pCreator, pstCtx->VdecChn);

    /*open the stream file*/
    file = fopen(pstCtx->aszFileName, "r");
    if (HI_NULL == file)
    {
        printf("open file %s err\n", pstCtx->aszFileName);
        exit(-1);
    }

    pu8VdecBuf = (HI_U8*)malloc(SAMPLE_PCIV_SEND_VDEC_LEN);
    HI_ASSERT(pu8VdecBuf);

    while(pstCtx->bThreadStart)
    {
        s32ReadLen = fread(pu8VdecBuf, 1, SAMPLE_PCIV_SEND_VDEC_LEN, file);
        if (s32ReadLen <= 0)
        {
            fseek(file, 0, SEEK_SET);/*read file again*/
            continue;
        }

        /* you should insure buf len is enough */
        PCIV_Trans_QueryLocBuf(pCreator, &stLocBufSta);
        if (stLocBufSta.u32FreeLen < s32ReadLen + sizeof(PCIV_STREAM_HEAD_S))
        {
            printf("venc stream local buffer not enough, %d < %d\n",stLocBufSta.u32FreeLen,s32ReadLen);
            break;
        }

        /* fill stream header info */
        stHeadTmp.u32Magic      = PCIV_STREAM_MAGIC;
        stHeadTmp.enPayLoad     = PT_H264;
        stHeadTmp.s32ChnID      = pstCtx->VdecChn;
        //stHeadTmp.u32DataLen    = s32ReadLen;
        stHeadTmp.u32Seq        = u32FrmSeq ++;

        /* write stream header */
        s32Ret = PCIV_Trans_WriteLocBuf(pCreator, (HI_U8*)&stHeadTmp, sizeof(stHeadTmp));
        HI_ASSERT((HI_SUCCESS == s32Ret));

        /* write stream data */
        s32Ret = PCIV_Trans_WriteLocBuf(pCreator, pu8VdecBuf, s32ReadLen);
        HI_ASSERT((HI_SUCCESS == s32Ret));

        /* send local data to pci target */
        while (PCIV_Trans_SendData(pCreator) && pstCtx->bThreadStart)
        {
            usleep(0);
        }
    }

    fclose(file);
    free(pu8VdecBuf);
    return NULL;
}

HI_S32 SamplePciv_HostInitWinVb(HI_S32 s32RmtChip)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_WINVB_S stWinVbArgs;

    stWinVbArgs.stPciWinVbCfg.u32PoolCount = 1;
    stWinVbArgs.stPciWinVbCfg.u32BlkCount[0] = 1;
    stWinVbArgs.stPciWinVbCfg.u32BlkSize[0] = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;

    memcpy(stMsg.cMsgBody, &stWinVbArgs, sizeof(stWinVbArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_WIN_VB;
    stMsg.stMsgHead.u32MsgLen = sizeof(stWinVbArgs);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_INIT_WIN_VB==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStartVdecChn(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VDEC_S stVdecArgs;

    stVdecArgs.VdecChn = VdecChn;
    stVdecArgs.enPicSize = SAMPLE_PCIV_VDEC_SIZE;
    memcpy(stMsg.cMsgBody, &stVdecArgs, sizeof(stVdecArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_START_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(stVdecArgs);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_START_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVdecChn(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VDEC_S stVdecArgs={0};

    stVdecArgs.VdecChn = VdecChn;
    memcpy(stMsg.cMsgBody, &stVdecArgs, sizeof(stVdecArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_STOP_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_STOP_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}


HI_S32 SamplePciv_HostStartVdecStream(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    //HI_S32 s32MsgPortWirte, s32MsgPortRead;
    SAMPLE_PCIV_MSG_S  stMsg;
    PCIV_PCIVCMD_MALLOC_S stMallocCmd;
    PCIV_PCIVCMD_MALLOC_S *pstMallocEcho;
    PCIV_TRANS_ATTR_S stInitPara;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &astPcivVdecCtx[VdecChn];

    /* send msg to slave(PCI Device), for malloc Dest Addr of stream buffer */
    /* PCI transfer data from Host to Device, Dest Addr must in PCI WINDOW of PCI Device */
    stMallocCmd.u32BlkCount = 1;
    stMallocCmd.u32BlkSize = SAMPLE_PCIV_VDEC_STREAM_BUF_LEN;
    memcpy(stMsg.cMsgBody, &stMallocCmd, sizeof(PCIV_PCIVCMD_MALLOC_S));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_MALLOC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_MALLOC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* read msg, phyaddr will return */
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    pstMallocEcho = (PCIV_PCIVCMD_MALLOC_S *)stMsg.cMsgBody;
    printf("func:%s, phyaddr:0x%x\n", __FUNCTION__, pstMallocEcho->u32PhyAddr[0]);

    /* init vdec stream sender in local chip */
    stInitPara.s32RmtChip       = s32RmtChip;
    stInitPara.s32ChnId         = VdecChn;
    stInitPara.u32BufSize       = pstMallocEcho->u32BlkSize;
    stInitPara.u32PhyAddr       = pstMallocEcho->u32PhyAddr[0] + g_u32PfWinBase[s32RmtChip];
    stInitPara.s32MsgPortWrite  = pstVdecCtx->s32MsgPortWrite;
    stInitPara.s32MsgPortRead   = pstVdecCtx->s32MsgPortRead;
    s32Ret = PCIV_Trans_InitSender(&stInitPara, &pstVdecCtx->pTransHandle);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    /* send msg to slave chip to init vdec stream transfer */
    stInitPara.s32RmtChip   = 0;
    stInitPara.u32PhyAddr   = pstMallocEcho->u32PhyAddr[0];
    memcpy(stMsg.cMsgBody, &stInitPara, sizeof(stInitPara));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_STREAM_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(stInitPara);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_INIT_STREAM_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* after target inited stream receiver, local start sending stream thread */
    sprintf(pstVdecCtx->aszFileName, "%s", SAMPLE_PCIV_VDEC_FILE);
    pstVdecCtx->VdecChn = VdecChn;
    pstVdecCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVdecCtx->pid, NULL, SamplePciv_SendVdStrmThread, pstVdecCtx);

    printf("init vdec:%d stream transfer ok ==================\n", VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVdecStream(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    PCIV_TRANS_ATTR_S stInitPara;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &astPcivVdecCtx[VdecChn];

    /* send msg to slave chip to exit vdec stream transfer */
    stInitPara.s32RmtChip   = 0;
    stInitPara.s32ChnId     = VdecChn;
    memcpy(stMsg.cMsgBody, &stInitPara, sizeof(stInitPara));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(stInitPara);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while( PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* exit local sending pthread */
    if (HI_TRUE == pstVdecCtx->bThreadStart)
    {
        pstVdecCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVdecCtx->pid, 0);
    }

    /* exit local sender */
    s32Ret = PCIV_Trans_DeInitSender(pstVdecCtx->pTransHandle);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    printf("exit vdec:%d stream transfer ok ==================\n", VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartVdecByChip(HI_S32 s32RmtChipId, HI_U32 u32VdecCnt)
{
    HI_S32 s32Ret, j;
    s32Ret = SamplePciv_HostInitWinVb(s32RmtChipId);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }
    for (j=0; j<u32VdecCnt; j++)
    {
        s32Ret  = SamplePciv_HostStartVdecChn(s32RmtChipId, j);
        s32Ret += SamplePciv_HostStartVdecStream(s32RmtChipId, j);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    return HI_SUCCESS;
}
HI_VOID SamplePciv_StopVdecByChip(HI_S32 s32RmtChipId, HI_U32 u32VdecCnt)
{
    HI_S32 j;
    for (j=0; j<u32VdecCnt; j++)
    {
        SamplePciv_HostStopVdecStream(s32RmtChipId, j);
        SamplePciv_HostStopVdecChn(s32RmtChipId, j);
    }
}

HI_S32 SamplePciv_HostStartVpssChn(HI_S32 s32RmtChip, VPSS_GRP VpssGrp, int vdec_flag)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VPSS_S stVpssArgs = {0};

    stVpssArgs.vpssGrp = VpssGrp;
    stVpssArgs.enPicSize = PIC_D1;
    if (0 == vdec_flag)
    {
        stVpssArgs.stBInd.enModId = HI_ID_VIU;
    }
    else
    {
        stVpssArgs.stBInd.enModId = HI_ID_VDEC;
    }
    stVpssArgs.stBInd.s32DevId = 0;
    stVpssArgs.stBInd.s32ChnId = VpssGrp;
    stVpssArgs.vpssChnStart[VPSS_BSTR_CHN] = HI_TRUE;
    if (1 == test_idx)
    {
        stVpssArgs.vpssChnStart[VPSS_PRE0_CHN] = HI_TRUE;
    }
    else if (2 == test_idx)
    {
        stVpssArgs.vpssChnStart[VPSS_BYPASS_CHN] = HI_TRUE;
    }

    memcpy(stMsg.cMsgBody, &stVpssArgs, sizeof(stVpssArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_START_VPSS;
    stMsg.stMsgHead.u32MsgLen = sizeof(stVpssArgs);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_START_VPSS==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVpssChn(HI_S32 s32RmtChip, VPSS_GRP VpssGrp, int vdec_flag)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VPSS_S stVpssArgs = {0};

    stVpssArgs.vpssGrp = VpssGrp;
    stVpssArgs.enPicSize = PIC_D1;
    if (0 == vdec_flag)
    {
        stVpssArgs.stBInd.enModId = HI_ID_VIU;
    }
    else
    {
        stVpssArgs.stBInd.enModId = HI_ID_VDEC;
    }
    stVpssArgs.stBInd.s32DevId = 0;
    stVpssArgs.stBInd.s32ChnId = 0;//VpssGrp;
    stVpssArgs.vpssChnStart[VPSS_BSTR_CHN] = HI_TRUE;
    if (1 == test_idx)
    {
        stVpssArgs.vpssChnStart[VPSS_PRE0_CHN] = HI_TRUE;
    }
    else if (2 == test_idx)
    {
        stVpssArgs.vpssChnStart[VPSS_BYPASS_CHN] = HI_TRUE;
    }
    memcpy(stMsg.cMsgBody, &stVpssArgs, sizeof(stVpssArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_STOP_VPSS;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_STOP_VPSS==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartVpssByChip(HI_S32 s32RmtChipId, HI_U32 u32VpssCnt, int vdec_flag)
{
    HI_S32 s32Ret, j;

    for (j=0; j<u32VpssCnt; j++)
    {
        s32Ret  = SamplePciv_HostStartVpssChn(s32RmtChipId, j, vdec_flag);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    return HI_SUCCESS;
}
HI_VOID SamplePciv_StopVpssByChip(HI_S32 s32RmtChipId, HI_U32 u32VpssCnt, int vdec_flag)
{
    HI_S32 j;

    for (j=0; j<u32VpssCnt; j++)
    {
        SamplePciv_HostStopVpssChn(s32RmtChipId, j, vdec_flag);
    }
}

HI_S32 SamplePciv_HostInitVi(HI_S32 s32RmtChip, HI_S32 s32ViCnt, PIC_SIZE_E enPicSize)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S      stMsg;
    SAMPLE_PCIV_MSG_INIT_VI_S stViArg;

    stViArg.s32ViChnCnt = s32ViCnt;
    stViArg.enPicSize = enPicSize;
    stViArg.enViDevType = VI_DEV_BT656_D1_4MUX;
#ifdef Board_Test_B
    switch (stViArg.enViDevType)
    {
        case VI_DEV_BT656_D1_1MUX:
            s32Ret = SAMPLE_TW2865_CfgV(gs_enViNorm,DEV_ATTR_BT656D1_1MUX.enWorkMode);
            if (s32Ret != HI_SUCCESS)
            {
                printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                   __FUNCTION__,  s32Ret);
                return HI_FAILURE;
            }
            break;
        case VI_DEV_BT656_D1_4MUX:
            s32Ret = SAMPLE_TW2865_CfgV(gs_enViNorm,DEV_ATTR_BT656D1_4MUX.enWorkMode);
            if (s32Ret != HI_SUCCESS)
            {
                printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                   __FUNCTION__,  s32Ret);
                return HI_FAILURE;
            }
            break;
        case VI_DEV_BT656_960H_1MUX:
            s32Ret = SAMPLE_TW2960_CfgV(gs_enViNorm,DEV_ATTR_BT656D1_1MUX.enWorkMode);
            if (s32Ret != HI_SUCCESS)
            {
                printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                   __FUNCTION__,  s32Ret);
                return HI_FAILURE;
            }
            break;
        case VI_DEV_BT656_960H_4MUX:
            s32Ret = SAMPLE_TW2960_CfgV(gs_enViNorm,DEV_ATTR_BT656D1_4MUX.enWorkMode);
            if (s32Ret != HI_SUCCESS)
            {
                printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                   __FUNCTION__,  s32Ret);
                return HI_FAILURE;
            }
            break;
        default:
            printf("vi input type[%d] is invalid!\n", stViArg.enViDevType);
            return HI_FAILURE;
    }
#endif
    /* send msg to slave chip to init vi */
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_VI;
    stMsg.stMsgHead.u32MsgLen = sizeof(SAMPLE_PCIV_MSG_INIT_VI_S);
    memcpy(stMsg.cMsgBody, &stViArg, sizeof(SAMPLE_PCIV_MSG_INIT_VI_S));
    printf("\nSend message (init vi) to slave!\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);
    printf("Slave init vi succeed!\n");
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostExitVi(HI_S32 s32RmtChip, HI_S32 s32ViCnt)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S      stMsg;
    SAMPLE_PCIV_MSG_INIT_VI_S stViArg;

    stViArg.s32ViChnCnt = s32ViCnt;

    /* send msg to slave chip to exit vi */
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_VI;
    stMsg.stMsgHead.u32MsgLen = sizeof(SAMPLE_PCIV_MSG_INIT_VI_S);
    memcpy(stMsg.cMsgBody, &stViArg, sizeof(SAMPLE_PCIV_MSG_INIT_VI_S));
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_VI==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    printf("exit all vi ok \n");
    return HI_SUCCESS;
}

void VDEC_MST_DefaultH264HD_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
    pstAttr->enType=PT_H264;
    pstAttr->u32BufSize=2*1920*1080;
    pstAttr->u32Priority=5;
    pstAttr->u32PicWidth=1920;
    pstAttr->u32PicHeight=1080;
    pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_STREAM;
    pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
    pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}
void VDEC_MST_DefaultH264D1_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
	pstAttr->enType=PT_H264;
	pstAttr->u32BufSize=2*720*576;
	pstAttr->u32Priority=5;
	pstAttr->u32PicWidth=720;
	pstAttr->u32PicHeight=576;
	pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_FRAME;
	pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
	pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}
void VDEC_MST_DefaultH264960H_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
	pstAttr->enType=PT_H264;
	pstAttr->u32BufSize=2*960*540;
	pstAttr->u32Priority=5;
	pstAttr->u32PicWidth=960;
	pstAttr->u32PicHeight=540;
	pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_STREAM;
	pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
	pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}
HI_S32 SamplePciv_GetDefVoAttr(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, VO_PUB_ATTR_S *pstPubAttr,
    VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, HI_S32 s32SquareSort, VO_CHN_ATTR_S *astChnAttr)
{
    VO_INTF_TYPE_E enIntfType;
    HI_U32 u32Frmt, u32Width, u32Height, j;

    switch (VoDev)
    {
        default:
        case 0: enIntfType = VO_INTF_VGA; break;
        case 1: enIntfType = VO_INTF_BT1120; break;
        case 2: enIntfType = VO_INTF_CVBS; break;
        case 3: enIntfType = VO_INTF_CVBS; break;
    }

    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL      :  u32Width = 720;  u32Height = 576;  u32Frmt = 25; break;
        case VO_OUTPUT_NTSC     :  u32Width = 720;  u32Height = 480;  u32Frmt = 30; break;
        case VO_OUTPUT_1080P24  :  u32Width = 1920; u32Height = 1080; u32Frmt = 24; break;
        case VO_OUTPUT_1080P25  :  u32Width = 1920; u32Height = 1080; u32Frmt = 25; break;
        case VO_OUTPUT_1080P30  :  u32Width = 1920; u32Height = 1080; u32Frmt = 30; break;
        case VO_OUTPUT_720P50   :  u32Width = 1280; u32Height = 720;  u32Frmt = 50; break;
        case VO_OUTPUT_720P60   :  u32Width = 1280; u32Height = 720;  u32Frmt = 60; break;
        case VO_OUTPUT_1080I50  :  u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080I60  :  u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_1080P50  :  u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080P60  :  u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_576P50   :  u32Width = 720;  u32Height = 576;  u32Frmt = 50; break;
        case VO_OUTPUT_480P60   :  u32Width = 720;  u32Height = 480;  u32Frmt = 60; break;
        case VO_OUTPUT_800x600_60: u32Width = 800;  u32Height = 600;  u32Frmt = 60; break;
        case VO_OUTPUT_1024x768_60:u32Width = 1024; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x1024_60:u32Width =1280; u32Height = 1024; u32Frmt = 60; break;
        case VO_OUTPUT_1366x768_60:u32Width = 1366; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1440x900_60:u32Width = 1440; u32Height = 900;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x800_60:u32Width = 1280; u32Height = 800;  u32Frmt = 60; break;

        default: return HI_FAILURE;
    }

    if (NULL != pstPubAttr)
    {
        pstPubAttr->enIntfSync = enIntfSync;
        pstPubAttr->u32BgColor = 0; //0xFF; //BLUE
        pstPubAttr->bDoubleFrame = HI_FALSE;
        pstPubAttr->enIntfType = enIntfType;
    }

    if (NULL != pstLayerAttr)
    {
        pstLayerAttr->stDispRect.s32X       = 0;
        pstLayerAttr->stDispRect.s32Y       = 0;
        pstLayerAttr->stDispRect.u32Width   = u32Width;
        pstLayerAttr->stDispRect.u32Height  = u32Height;
        pstLayerAttr->stImageSize.u32Width  = u32Width;
        pstLayerAttr->stImageSize.u32Height = u32Height;
        pstLayerAttr->u32DispFrmRt          = 25;
        pstLayerAttr->enPixFormat           = SAMPLE_PIXEL_FORMAT;
    }

    if (NULL != astChnAttr)
    {
        for (j=0; j<(s32SquareSort * s32SquareSort); j++)
        {
            astChnAttr[j].stRect.s32X       = ALIGN_BACK((u32Width/s32SquareSort) * (j%s32SquareSort), 4);
            astChnAttr[j].stRect.s32Y       = ALIGN_BACK((u32Height/s32SquareSort) * (j/s32SquareSort), 4);
            astChnAttr[j].stRect.u32Width   = ALIGN_BACK(u32Width/s32SquareSort, 4);
            astChnAttr[j].stRect.u32Height  = ALIGN_BACK(u32Height/s32SquareSort, 4);
            astChnAttr[j].u32Priority       = 0;
            astChnAttr[j].bDeflicker        = HI_FALSE;
        }
    }

    return HI_SUCCESS;
}
HI_S32 SamplePciv_StartVO(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr,
    VO_VIDEO_LAYER_ATTR_S *astLayerAttr, VO_CHN_ATTR_S *astChnAttr,
    HI_S32 s32ChnNum, HI_BOOL bPip)
{
    HI_S32 i, s32Ret;

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Disable(VoDev);
    PCIV_CHECK_ERR(s32Ret);    

    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Enable(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_SetVideoLayerAttr(VoDev, &astLayerAttr[0]);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_EnableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    if (bPip)
    {
        s32Ret = HI_MPI_VO_PipLayerBindDev(VoDev);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VO_SetPipLayerAttr(&astLayerAttr[1]);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VO_EnablePipLayer();
        PCIV_CHECK_ERR(s32Ret);
    }

    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VO_SetChnAttr(VoDev, i, &astChnAttr[i]);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VO_EnableChn(VoDev, i);
        PCIV_CHECK_ERR(s32Ret);
    }

    return 0;
}
HI_S32 SamplePciv_StopVO(VO_DEV VoDev, HI_S32 s32ChnNum, HI_BOOL bPip)
{
    HI_S32 i, s32Ret;

    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoDev, i);
        PCIV_CHECK_ERR(s32Ret);
    }

    if (bPip)
    {
        s32Ret = HI_MPI_VO_DisablePipLayer();
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VO_PipLayerUnBindDev(VoDev);
        PCIV_CHECK_ERR(s32Ret);
    }

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Disable(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    return 0;
}
HI_S32 SamplePciv_HostCreateVdecVpssVo(HI_U32 s32ChnNum,PIC_SIZE_E enPicSize)
{
    HI_S32 i, s32Ret;
    VDEC_CHN_ATTR_S stVdecAttr;
    VPSS_GRP_ATTR_S stGrpAttr;
    VPSS_CHN VpssChn = VPSS_PRE0_CHN;
    MPP_CHN_S stSrcChn, stDestChn;

    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN_ATTR_S astChnAttr_16[16];
    VO_DEV VoDev = 0;

    SamplePciv_GetDefVoAttr(VoDev, VO_OUTPUT_1080P60, &stPubAttr, &stLayerAttr, 4, astChnAttr_16);
    s32Ret = SamplePciv_StartVO(VoDev, &stPubAttr, &stLayerAttr, astChnAttr_16, s32ChnNum, HI_FALSE);
    PCIV_CHECK_ERR(s32Ret);

    switch (enPicSize)
    {
        case PIC_D1     :
            VDEC_MST_DefaultH264D1_Attr(&stVdecAttr);

            stGrpAttr.u32MaxW = 720;
            stGrpAttr.u32MaxH = 576;
            break;
        case PIC_HD1080 :
            VDEC_MST_DefaultH264HD_Attr(&stVdecAttr);

            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1088;
            break;
        case PIC_960H :
            VDEC_MST_DefaultH264960H_Attr(&stVdecAttr);

            stGrpAttr.u32MaxW = 960;
            stGrpAttr.u32MaxH = 576;
            break;
        default         :
            VDEC_MST_DefaultH264D1_Attr(&stVdecAttr);

            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1088;
            break;
    }

    stGrpAttr.enPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    stGrpAttr.bDbEn     = 0;
    stGrpAttr.bDrEn     = 0;
    stGrpAttr.bHistEn   = 0;
    stGrpAttr.bIeEn     = 0;
    stGrpAttr.bNrEn     = 0;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VDEC_CreateChn(i, &stVdecAttr);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VDEC_StartRecvStream(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_CreateGrp(i, &stGrpAttr);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_StartGrp(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_EnableChn(i,VpssChn);
        PCIV_CHECK_ERR(s32Ret);

        stSrcChn.enModId = HI_ID_VDEC;
        stSrcChn.s32ChnId = i;
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = i;
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);

        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = VpssChn;
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}
HI_S32 SamplePciv_HostDestroyVdecVpssVo(HI_U32 s32ChnNum)
{
    HI_S32 i, s32Ret;
    VPSS_CHN VpssChn = VPSS_PRE0_CHN;
    VO_DEV VoDev = 2;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i=0; i<s32ChnNum; i++)
    {
        stSrcChn.enModId = HI_ID_VDEC;
        stSrcChn.s32ChnId = i;
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = i;
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);

        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = VpssChn;
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VDEC_StopRecvStream(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VDEC_DestroyChn(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_StopGrp(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_DisableChn(i,VpssChn);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VPSS_DestroyGrp(i);
        PCIV_CHECK_ERR(s32Ret);
    }
    SamplePciv_StopVO(VoDev, s32ChnNum, HI_FALSE);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStartVenc(HI_S32 s32RmtChip, HI_U32 u32GrpCnt,
    PIC_SIZE_E aenPicSize[2], HI_BOOL bHaveMinor)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S      stMsg;
    PCIV_VENCCMD_INIT_S stVencMsg;
    HI_U32 au32PhyAddr[4];
    HI_U32 u32BufLen = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    PCIV_TRANS_ATTR_S *pstStreamArgs;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stPcivVencCtx;

    if (!u32GrpCnt) return HI_SUCCESS;

    /* config venc chn info */
    stVencMsg.u32GrpCnt = u32GrpCnt;
    stVencMsg.bHaveMinor = bHaveMinor;
    stVencMsg.aenSize[0] = aenPicSize[0];
    stVencMsg.aenSize[1] = aenPicSize[1];
    
    stVencMsg.aenType[0] = PT_H264;
    stVencMsg.aenType[1] = PT_H264;
    
    
    if (0 == test_idx)
    {
        stVencMsg.bUseVpss   = HI_FALSE;
    }
    else
    {
        stVencMsg.bUseVpss   = HI_TRUE;
    }

    /* alloc phy buf for DMA receive stream data */
    s32Ret = HI_MPI_PCIV_Malloc(u32BufLen, 1, au32PhyAddr);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    /* config venc stream transfer info */
    pstStreamArgs = &stVencMsg.stStreamArgs;
    pstStreamArgs->u32BufSize = u32BufLen;
    pstStreamArgs->u32PhyAddr = au32PhyAddr[0];
    pstStreamArgs->s32RmtChip = s32RmtChip;
    printf("\nMalloc local buf succed! Address is 0x%x.\n", au32PhyAddr[0]);
    /* notes:msg port have open when init */
    pstStreamArgs->s32MsgPortWrite = pstVencCtx->s32MsgPortWrite;
    pstStreamArgs->s32MsgPortRead  = pstVencCtx->s32MsgPortRead;

    /* init stream recerver */
    s32Ret = PCIV_Trans_InitReceiver(pstStreamArgs, &pstVencCtx->pTransHandle);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* send msg to slave chip to create venc chn and init stream transfer */
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_ALL_VENC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_VENCCMD_INIT_S);
    memcpy(stMsg.cMsgBody, &stVencMsg, sizeof(PCIV_VENCCMD_INIT_S));
    printf("\nSend (start venc) message to slave!\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    printf("Pciv venc init ok, remote:%d, grpcnt:%d =======\n", s32RmtChip, u32GrpCnt);

    /* create thread to reveive venc stream from slave chip */
    pstVencCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVencCtx->pid, NULL, SamplePciv_RevVeStrmThread, pstVencCtx);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVenc(HI_S32 s32TargetId, HI_U32 u32GrpCnt, HI_BOOL bHaveMinor)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S   stMsg;
    PCIV_VENCCMD_EXIT_S stVencMsg;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stPcivVencCtx;

    if (!u32GrpCnt) return HI_SUCCESS;

    stVencMsg.u32GrpCnt = u32GrpCnt;
    stVencMsg.bHaveMinor = bHaveMinor;
    if (0 == test_idx)
    {
        stVencMsg.bUseVpss = HI_FALSE;
    }
    else
    {
        stVencMsg.bUseVpss = HI_TRUE;
    }
    /* send msg to slave chip to exit all venc chn */
    stMsg.stMsgHead.u32Target = s32TargetId;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_ALL_VENC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_VENCCMD_INIT_S);
    memcpy(stMsg.cMsgBody, &stVencMsg, sizeof(PCIV_VENCCMD_EXIT_S));
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_ALL_VENC==========\n");
    s32Ret = PCIV_SendMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* exit the thread of getting venc stream */
    if (pstVencCtx->bThreadStart)
    {
        pstVencCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVencCtx->pid, 0);
    }

    /* deinit venc stream receiver */
    PCIV_Trans_DeInitReceiver(pstVencCtx->pTransHandle);

    printf("pciv venc exit ok !========= \n");
    return HI_SUCCESS;
}

HI_S32 SamplePcivGetBlkSize(PCIV_PIC_ATTR_S *pstPicAttr, HI_U32 *pu32BlkSize)
{
    switch (pstPicAttr->enPixelFormat)
    {
        case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height*3/2;
            break;
        case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height*2;
            break;
        case PIXEL_FORMAT_VYUY_PACKAGE_422:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height;
            break;
        default:
            return -1;
    }
    return 0;
}

pthread_t g_Thread;
HI_BOOL g_Run = HI_FALSE;
HI_BOOL g_Exist = HI_FALSE;
void *PTHREAD_SelectGetMultiStream(void *pData)
{
    HI_S32 i,j;
    HI_S32 maxfd = 0;
    HI_S32 s32Ret;
    HI_U32 u32ChnNum = 16;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64] = {0};
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stVeStream;
    VENC_PACK_S stVePack[5];

    struct timeval TimeoutVal;
    fd_set read_fds;
    FILE *pFile[VENC_MAX_CHN_NUM] = {0};

    for (i=4; i<u32ChnNum; i++)
    {
        /* decide the stream file name, and open file to save stream */
        sprintf(aszFileName[i], "./pciv_chn%d.h264", i);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i])
        {
            printf("open file %s err! line: %d\n", aszFileName[i], __LINE__);
            return NULL;
        }
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] <= 0)
        {
            continue;
        }

        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /* Start to get streams of each channel. */
    while (HI_TRUE == g_Run)
    {
        usleep(1000);

        for (i = 4; i < u32ChnNum; i++)
        {
            memset(&stVeStream, 0, sizeof(stVeStream));
            s32Ret = HI_MPI_VENC_Query(i, &stStat);
            if (s32Ret != HI_SUCCESS)
            {
                continue;
            }
            //printf("HI_MPI_VENC_Query %d ok\n", i);

            stVeStream.pstPack = stVePack;
            stVeStream.u32PackCount = 5;
            s32Ret = HI_MPI_VENC_GetStream(i, &stVeStream, HI_IO_BLOCK);
            if (s32Ret != HI_SUCCESS)
            {
                printf("HI_MPI_VENC_GetStream err 0x%x\n", s32Ret);
                continue;
            }
            //printf("HI_MPI_VENC_GetStream %d ok\n", i);

            /* step 4: save stream. */                        
            if (NULL != pFile[i])
			{
				for (j=0; j< stVeStream.u32PackCount; j++)
				{
					fwrite(stVeStream.pstPack[j].pu8Addr[0], stVeStream.pstPack[j].u32Len[0], sizeof(HI_U8), pFile[i]);

					if (stVeStream.pstPack[j].u32Len[1] > 0)
					{
						fwrite(stVeStream.pstPack[j].pu8Addr[1], stVeStream.pstPack[j].u32Len[1], sizeof(HI_U8), pFile[i]);
					}
                    fflush(pFile[i]);
				}
			}

            /* step 5: release these stream */
            s32Ret = HI_MPI_VENC_ReleaseStream(i, &stVeStream);
            if (s32Ret != HI_SUCCESS)
            {
                continue;
            }
            //printf("HI_MPI_VENC_ReleaseStream %d ok\n", i);
        }
    }

    for (i = 0; i < u32ChnNum; i++)
    {
        if (pFile[i])
        {
            fclose(pFile[i]);
        }
    }

    printf("exit venc thread ok\n");
    return NULL;
}
HI_S32 SamplePciv_HostStartPciv(PCIV_CHN PcivChn, PCIV_REMOTE_OBJ_S *pstRemoteObj,
        PCIV_BIND_OBJ_S *pstLocBind, PCIV_BIND_OBJ_S *pstRmtBind, PCIV_PIC_ATTR_S *pstPicAttr)
{
    HI_S32                s32Ret;
    PCIV_ATTR_S           stPcivAttr;
    PCIV_PCIVCMD_CREATE_S stMsgCreate;
    MPP_CHN_S             stSrcChn, stDestChn;
    
    stPcivAttr.stRemoteObj.s32ChipId = pstRemoteObj->s32ChipId;
    stPcivAttr.stRemoteObj.pcivChn = pstRemoteObj->pcivChn;

    /* memcpy pciv pic attr */
    memcpy(&stPcivAttr.stPicAttr, pstPicAttr, sizeof(PCIV_PIC_ATTR_S));

    /* 1) config pic buffer info, count/size/addr */
    stPcivAttr.u32Count = 4;
    SamplePcivGetBlkSize(&stPcivAttr.stPicAttr, &stPcivAttr.u32BlkSize);
    s32Ret = HI_MPI_PCIV_Malloc(stPcivAttr.u32BlkSize, stPcivAttr.u32Count, stPcivAttr.u32PhyAddr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv malloc err, size:%d, count:%d\n", stPcivAttr.u32BlkSize, stPcivAttr.u32Count);
        return s32Ret;
    }

    /* 2) create pciv chn */
    s32Ret = HI_MPI_PCIV_Create(PcivChn, &stPcivAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d create failed \n", PcivChn);
        return s32Ret;
    }

#ifdef HOST_PCIV_BIND_VENC
{
    VENC_CHN_ATTR_S stVencAttr = {0};
    VENC_GRP VeGroup = PcivChn;
    VENC_CHN VeChn = PcivChn;

    stVencAttr.stVeAttr.enType = PT_H264;
    stVencAttr.stVeAttr.stAttrH264e.u32MaxPicWidth = pstPicAttr->u32Width;
    stVencAttr.stVeAttr.stAttrH264e.u32MaxPicHeight = pstPicAttr->u32Height;
    stVencAttr.stVeAttr.stAttrH264e.u32PicWidth = pstPicAttr->u32Width;/*the picture width*/
    stVencAttr.stVeAttr.stAttrH264e.u32PicHeight = pstPicAttr->u32Height;/*the picture height*/
    stVencAttr.stVeAttr.stAttrH264e.u32BufSize  = pstPicAttr->u32Width * pstPicAttr->u32Height * 2;/*stream buffer size*/
    stVencAttr.stVeAttr.stAttrH264e.u32Profile  = 0;/*0: baseline; 1:MP; 2:HP   ? */
    stVencAttr.stVeAttr.stAttrH264e.bByFrame = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
    stVencAttr.stVeAttr.stAttrH264e.bField = HI_FALSE;  /* surpport frame code only for hi3516, bfield = HI_FALSE */
    stVencAttr.stVeAttr.stAttrH264e.bMainStream = HI_TRUE; /* surpport main stream only for hi3516, bMainStream = HI_TRUE */
    stVencAttr.stVeAttr.stAttrH264e.u32Priority = 0; /*channels precedence level. invalidate for hi3516*/
    stVencAttr.stVeAttr.stAttrH264e.bVIField = HI_FALSE;/*the sign of the VI picture is field or frame. Invalidate for hi3516*/

    stVencAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    stVencAttr.stRcAttr.stAttrH264Cbr.u32Gop            = (VIDEO_ENCODING_MODE_PAL== g_enViNorm)?25:30;
    stVencAttr.stRcAttr.stAttrH264Cbr.u32StatTime       = 1; /* stream rate statics time(s) */
    stVencAttr.stRcAttr.stAttrH264Cbr.u32ViFrmRate      = (VIDEO_ENCODING_MODE_PAL== g_enViNorm)?25:30;/* input (vi) frame rate */
    stVencAttr.stRcAttr.stAttrH264Cbr.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== g_enViNorm)?25:30;/* target frame rate */
    stVencAttr.stRcAttr.stAttrH264Cbr.u32BitRate        = 512;
    stVencAttr.stRcAttr.stAttrH264Cbr.u32FluctuateLevel = 0;/* target frame rate */

    s32Ret = HI_MPI_VENC_CreateGroup(VeGroup);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc group %d create failed \n", VeGroup);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_CreateChn(VeChn, &stVencAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d create failed \n", PcivChn);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_RegisterChn(VeGroup, VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn (%d,%d) register failed \n", VeGroup, VeChn);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_StartRecvPic(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d StartRecvPic failed \n", VeChn);
        return s32Ret;
    }

    g_Run = HI_TRUE;
    if (HI_TRUE != g_Exist)
    {
        if (0 == pthread_create(&g_Thread, 0, PTHREAD_SelectGetMultiStream, NULL))
        {
            g_Exist = HI_TRUE;
            printf("pthread_create %d SUCCESS \n",PcivChn);
        }
        else
        {
            printf("pthread_create %d FAILED \n",PcivChn);
            perror("pthread_create");
        }
    }
}
#endif

    /* 3) pciv chn bind vo chn (for display pic in host)*/
    stSrcChn.enModId  = HI_ID_PCIV;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = PcivChn;
    switch (pstLocBind->enType)
    {
        case PCIV_BIND_VO:
            stDestChn.enModId  = HI_ID_VOU;
            stDestChn.s32DevId = pstLocBind->unAttachObj.voDevice.voDev;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VPSS:
            stDestChn.enModId  = HI_ID_VPSS;
            stDestChn.s32DevId = pstLocBind->unAttachObj.vpssDevice.vpssGrp;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            printf("pstLocBind->enType = %d\n",pstLocBind->enType);
            HI_ASSERT(0);
    }
#ifdef HOST_PCIV_BIND_VENC
    stDestChn.enModId  = HI_ID_GROUP;
    stDestChn.s32DevId = PcivChn;
    stDestChn.s32ChnId = 0;
#endif
    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d bind err. s32Ret=%#x\n", PcivChn,s32Ret);
        printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
            stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
            stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
        return s32Ret;
    }

    /* 4) start pciv chn (now vo will display pic from slave chip) */
    s32Ret = HI_MPI_PCIV_Start(PcivChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d start err \n", PcivChn);
        return s32Ret;
    }

    printf("pciv chn%d start ok, remote(%d,%d), bindvo(%d,%d); then send msg to slave chip !\n",
        PcivChn, stPcivAttr.stRemoteObj.s32ChipId,stPcivAttr.stRemoteObj.pcivChn,
        pstLocBind->unAttachObj.voDevice.voDev, pstLocBind->unAttachObj.voDevice.voChn);

    /* 5) send msg to slave chip to start picv chn ========================================*/

    stMsgCreate.pcivChn = pstRemoteObj->pcivChn;
    memcpy(&stMsgCreate.stDevAttr, &stPcivAttr, sizeof(stPcivAttr));
    /* reconfig remote obj for slave device */
    stMsgCreate.stDevAttr.stRemoteObj.s32ChipId = 0;
    stMsgCreate.stDevAttr.stRemoteObj.pcivChn   = PcivChn;
    stMsgCreate.bAddOsd                         = Add_osd;
    /* bind object of remote dev */
    memcpy(&stMsgCreate.stBindObj[0], pstRmtBind, sizeof(PCIV_BIND_OBJ_S));

    /* send msg */
    s32Ret = SamplePcivSendMsgCreate(pstRemoteObj->s32ChipId, &stMsgCreate);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }
    printf("send msg to slave chip to start pciv chn %d ok! \n\n", PcivChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopPciv(PCIV_CHN PcivChn, PCIV_BIND_OBJ_S *pstLocBind, PCIV_BIND_OBJ_S *pstRmtBind)
{
    HI_S32 s32Ret;
    PCIV_ATTR_S  stPciAttr;
    PCIV_PCIVCMD_DESTROY_S stMsgDestroy;
    MPP_CHN_S stSrcChn, stDestChn;

    /* 1, stop */
    s32Ret = HI_MPI_PCIV_Stop(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

    /* 2, unbind */
    stSrcChn.enModId  = HI_ID_PCIV;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = PcivChn;
    switch (pstLocBind->enType)
    {
        case PCIV_BIND_VO:
            stDestChn.enModId  = HI_ID_VOU;
            stDestChn.s32DevId = pstLocBind->unAttachObj.voDevice.voDev;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VPSS:
            stDestChn.enModId  = HI_ID_VPSS;
            stDestChn.s32DevId = 0;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssGrp;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            printf("pstLocBind->enType = %d\n",pstLocBind->enType);
            HI_ASSERT(0);
    }
#ifdef HOST_PCIV_BIND_VENC
    stDestChn.enModId  = HI_ID_GROUP;
    stDestChn.s32DevId = PcivChn;
    stDestChn.s32ChnId = 0;
#endif
    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d bind err. s32Ret=%#x\n", PcivChn,s32Ret);
        printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
            stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
            stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
        return s32Ret;
    }

    /* 3, free */
    s32Ret = HI_MPI_PCIV_GetAttr(PcivChn, &stPciAttr);
    s32Ret = HI_MPI_PCIV_Free(stPciAttr.u32Count, stPciAttr.u32PhyAddr);
    PCIV_CHECK_ERR(s32Ret);

    /* 4, destroy */
    s32Ret = HI_MPI_PCIV_Destroy(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

#ifdef HOST_PCIV_BIND_VENC
{
    VENC_GRP VeGroup = PcivChn;
    VENC_CHN VeChn = PcivChn;

    g_Run = HI_FALSE;
    
    s32Ret = HI_MPI_VENC_StopRecvPic(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d StopRecvPic failed \n", VeChn);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_UnRegisterChn(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d StopRecvPic failed \n", VeChn);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_DestroyChn(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d Destroy failed \n", VeChn);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_DestroyGroup(VeGroup);
    if (s32Ret != HI_SUCCESS)
    {
        printf("venc chn %d Destroy failed \n", VeGroup);
        return s32Ret;
    }

    if (HI_TRUE == g_Exist)
    {
        pthread_join(g_Thread,0);
        g_Exist = HI_FALSE;
    }
}
#endif

    printf("start send msg to slave chip to destroy pciv chn %d\n", PcivChn);
    stMsgDestroy.pcivChn = PcivChn;
    memcpy(&stMsgDestroy.stBindObj[0], pstRmtBind, sizeof(PCIV_BIND_OBJ_S));
    s32Ret = SamplePcivSendMsgDestroy(stPciAttr.stRemoteObj.s32ChipId, &stMsgDestroy);
    PCIV_CHECK_ERR(s32Ret);
    printf("destroy pciv chn %d ok \n\n", PcivChn);

    return HI_SUCCESS;
}

/*
 * Start all pciv chn for one vo dev,
 * @u32RmtIdx: 对端PCI设备数组下标，用于计算PCIV通道号
 * @s32RmtChipId: 对端PCI设备号
 * @u32DispIdx: 本地输出显示s_astPcivDisp的序号，用于计算PCIV通道号
 * @pstDispCtx: 本地输出显示上下文，记录了指定VO设备中与PCIV绑定的通道范围，画面分割等
 */
HI_S32 SamplePciv_StartPcivByVo(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId,
        HI_U32 u32DispIdx, SAMPLE_PCIV_DISP_CTX_S *pstDispCtx)
{
    HI_S32 s32Ret, j, s32ChnCnt;
    PCIV_CHN RmtPcivChn, LocPcivChn;
    SIZE_S stPicSize;
    PCIV_REMOTE_OBJ_S stRemoteObj;
    PCIV_PIC_ATTR_S stPicAttr;

    s32ChnCnt = pstDispCtx->VoChnEnd - pstDispCtx->VoChnStart + 1;
    
    switch(s32ChnCnt)
    {
        case 76:
        case 100:
        case 114: 
        case 132:
        {
            s32ChnCnt = 16;
            break;
        }
        case 174:
        {
            s32ChnCnt = 9;
            break;
        }
        case 200:
        case 242:    
        {
            s32ChnCnt = 4;
            break;
        }
        case 300:
        case 320:
        case 640:    
        {
            s32ChnCnt = 1;
            break;
        }
        default:
            break;
        
    }
    
    printf("s32ChnCnt is %d.....\n", s32ChnCnt);
    HI_ASSERT(s32ChnCnt >= 0 && s32ChnCnt <= SAMPLE_PCIV_CNT_PER_VO);

    for (j=0; j<s32ChnCnt; j++)
    {
        /* 1) local pciv chn and remote pciv chn */
        LocPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO + u32RmtIdx*SAMPLE_PCIV_CNT_PER_DEV;
        RmtPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO;
        HI_ASSERT(LocPcivChn < PCIV_MAX_CHN_NUM);

        /* 2) remote dev and chn */
        stRemoteObj.s32ChipId   = s32RmtChipId;
        stRemoteObj.pcivChn     = RmtPcivChn;

        /* 3) local bind object */
        g_stLocBind[j].enType = PCIV_BIND_VO;
        g_stLocBind[j].unAttachObj.voDevice.voDev = pstDispCtx->VoDev;
        g_stLocBind[j].unAttachObj.voDevice.voChn = pstDispCtx->VoChnStart + j;

        /* 4) remote bind object */
        if (0 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VI;
            g_stRmtBind[j].unAttachObj.viDevice.viDev = 0;
            g_stRmtBind[j].unAttachObj.viDevice.viChn = j;
        }
        else if (1 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VPSS;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssGrp = j;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssChn = VPSS_PRE0_CHN;
        }
        else if (2 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VPSS;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssGrp = j;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssChn = VPSS_BYPASS_CHN;
        }
        else if (3 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VDEC;
            g_stRmtBind[j].unAttachObj.vdecDevice.vdecChn = j;
        }
        else
        {
            printf("test_idx:%d error.\n",test_idx);
            return HI_FAILURE;
        }

        /* 5) pciv pic attr */
        SamplePcivGetPicSize(pstDispCtx->u32PicDiv, &stPicSize);
        SamplePcivGetPicAttr(&stPicSize, &stPicAttr);

        /* start local and remote pciv chn */
        s32Ret = SamplePciv_HostStartPciv(LocPcivChn, &stRemoteObj, &g_stLocBind[j], &g_stRmtBind[j], &stPicAttr);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StopPcivByVo(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId,
        HI_U32 u32DispIdx, SAMPLE_PCIV_DISP_CTX_S *pstDispCtx)
{
    HI_S32 j, s32ChnCnt;
    PCIV_CHN LocPcivChn;

    s32ChnCnt = pstDispCtx->VoChnEnd - pstDispCtx->VoChnStart + 1;
    
    switch(s32ChnCnt)
    {
        case 76:
        case 100:
        case 114: 
        case 132:
        {
            s32ChnCnt = 16;
            break;
        }
        case 174:
        {
            s32ChnCnt = 9;
            break;
        }
        case 200:
        case 242:    
        {
            s32ChnCnt = 4;
            break;
        }
        case 300:
        case 320:
        case 640:    
        {
            s32ChnCnt = 1;
            break;
        }
        default:
            break;
        
    }
    
    printf("s32ChnCnt is %d.....\n", s32ChnCnt);
    HI_ASSERT(s32ChnCnt >= 0 && s32ChnCnt <= SAMPLE_PCIV_CNT_PER_VO);

    for (j=0; j<s32ChnCnt; j++)
    {
        LocPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO + u32RmtIdx*SAMPLE_PCIV_CNT_PER_DEV;
        HI_ASSERT(LocPcivChn < PCIV_MAX_CHN_NUM);

        SamplePciv_HostStopPciv(LocPcivChn, &g_stLocBind[j], &g_stRmtBind[j]);
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartPcivByChip(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId)
{
    HI_S32 s32Ret, i;

    for (i=0; i<VO_MAX_DEV_NUM; i++)
    {
        if (s_astPcivDisp[i].bValid != HI_TRUE)
        {
            continue;
        }
        s32Ret = SamplePciv_StartPcivByVo(u32RmtIdx,  s32RmtChipId, i, &s_astPcivDisp[i]);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StopPcivByChip(HI_S32 s32RmtChipId, HI_U32 u32RmtIdx)
{
    HI_S32 s32Ret, i;

    for (i=0; i<VO_MAX_DEV_NUM; i++)
    {
        if (s_astPcivDisp[i].bValid != HI_TRUE)
        {
            continue;
        }
        s32Ret = SamplePciv_StopPcivByVo(u32RmtIdx,  s32RmtChipId, i, &s_astPcivDisp[i]);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartVo()
{
    HI_S32 s32Ret, i;
    SAMPLE_PCIV_DISP_CTX_S *pstDispCtx = NULL;

    for (i=0; i<(sizeof(s_astPcivDisp)/sizeof(s_astPcivDisp[0])); i++)
    {
        pstDispCtx = &s_astPcivDisp[i];
        if (pstDispCtx->bValid != HI_TRUE)
            continue;
        printf("will start vo dev %d,cnt:%d,end:%d \n", pstDispCtx->VoDev,pstDispCtx->u32PicDiv,pstDispCtx->VoChnEnd);
        s32Ret = SAMPLE_StartVo_SD(pstDispCtx->u32PicDiv, pstDispCtx->VoDev);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}



/* get stream from venc chn, write to local buffer, then send to pci target,
    we send several stream frame one time, to improve PCI DMA efficiency  */
HI_VOID * SamplePciv_HostSendVencThread(HI_VOID *p)
{
    HI_S32           s32Ret, i, k;
    HI_S32           s32StreamCnt;
    HI_BOOL          bBufFull;
    VENC_CHN         vencChn = 0, VeChnBufFul = 0;
    HI_S32           s32Size, DMADataLen;
    VENC_STREAM_S    stVStream;
    VENC_PACK_S      astPack[128];
	HI_VOID         *pCreator    = NULL;
    PCIV_STREAM_HEAD_S      stHeadTmp;
    PCIV_TRANS_LOCBUF_STAT_S stLocBufSta;
    SAMPLE_PCIV_VENC_CTX_S *pstCtx = (SAMPLE_PCIV_VENC_CTX_S*)p;

    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];

    for (i=0; i<pstCtx->s32VencCnt; i++)
    {
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        HI_ASSERT(VencFd[i] > 0);
        if (maxfd <= VencFd[i]) maxfd = VencFd[i];
    }

    pCreator     = pstCtx->pTransHandle;
    s32StreamCnt = 0;
    bBufFull     = HI_FALSE;
    stVStream.pstPack = astPack;
    printf("%s -> Sender:%p, chncnt:%d\n", __FUNCTION__, pCreator, pstCtx->s32VencCnt);

    while (pstCtx->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i=0; i<pstCtx->s32VencCnt; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;  
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (!s32Ret) {printf("timeout\n"); continue;}
        HI_ASSERT(s32Ret >= 0);

        for (i=0; i<pstCtx->s32VencCnt; i++)
        {
            vencChn = i;

            /* if buf is not full last time, get venc stream from venc chn by noblock method*/
            if (HI_FALSE == bBufFull)
            {
                VENC_CHN_STAT_S stVencStat;
                if (!FD_ISSET(VencFd[vencChn], &read_fds))
                    continue;
                memset(&stVStream, 0, sizeof(stVStream));
                s32Ret = HI_MPI_VENC_Query(vencChn, &stVencStat);
                HI_ASSERT(HI_SUCCESS == s32Ret);
                stVStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stVencStat.u32CurPacks);
                HI_ASSERT(stVStream.pstPack);
                stVStream.u32PackCount = stVencStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(vencChn, &stVStream, HI_IO_BLOCK);
                HI_ASSERT(HI_SUCCESS == s32Ret);

                s32StreamCnt++;
                TraceStreamInfo(vencChn, &stVStream);/* only for DEBUG */
            }
            else/* else, use stVStream of last time */
            {
                bBufFull = HI_FALSE;
                vencChn = VeChnBufFul;
            }

            /* Caclute the data size, and check there is enough space in local buffer */
            s32Size = 0;
            for (k=0; k<stVStream.u32PackCount; k++)
            {
                s32Size += stVStream.pstPack[k].u32Len[0];
                s32Size += stVStream.pstPack[k].u32Len[1];
            }
            if (0 == (s32Size%4))
            {
                DMADataLen = s32Size;
            }
            else
            {
                DMADataLen = s32Size + (4 - (s32Size%4));
            }

            PCIV_Trans_QueryLocBuf(pCreator, &stLocBufSta);
            if (stLocBufSta.u32FreeLen < DMADataLen + sizeof(PCIV_STREAM_HEAD_S))
            {
                printf("venc stream local buffer not enough,chn:%d, %d < %d+%d \n",
                    vencChn, stLocBufSta.u32FreeLen, DMADataLen, sizeof(PCIV_STREAM_HEAD_S));
                bBufFull = HI_TRUE;
                VeChnBufFul = vencChn;
                break;
            }

            /* fill stream header info */
            stHeadTmp.u32Magic  = PCIV_STREAM_MAGIC;
            stHeadTmp.enPayLoad = PT_H264;
            stHeadTmp.s32ChnID   = vencChn;
            stHeadTmp.u32StreamDataLen = s32Size;
            stHeadTmp.u32DMADataLen = DMADataLen;
            stHeadTmp.u32Seq    = stVStream.u32Seq;
            stHeadTmp.u64PTS    = stVStream.pstPack[0].u64PTS;
            stHeadTmp.bFieldEnd = stVStream.pstPack[0].bFieldEnd;
            stHeadTmp.bFrameEnd = stVStream.pstPack[0].bFrameEnd;
            stHeadTmp.enDataType= stVStream.pstPack[0].DataType;

            /* write stream header */
            s32Ret = PCIV_Trans_WriteLocBuf(pCreator, (HI_U8*)&stHeadTmp, sizeof(stHeadTmp));
            HI_ASSERT((HI_SUCCESS == s32Ret));

            /* write stream data */
            for (k=0; k<stVStream.u32PackCount; k++)
            {
                s32Ret = PCIV_Trans_WriteLocBuf(pCreator,
                    stVStream.pstPack[k].pu8Addr[0], stVStream.pstPack[k].u32Len[0]);
                s32Ret += PCIV_Trans_WriteLocBuf(pCreator,
                    stVStream.pstPack[k].pu8Addr[1], stVStream.pstPack[k].u32Len[1]);
                HI_ASSERT((HI_SUCCESS == s32Ret));
            }
            PCIV_TRANS_SENDER_S *pstSender = (PCIV_TRANS_SENDER_S*)pCreator;
            if (0 != pstSender->stLocBuf.u32CurLen%4)
            {
                pstSender->stLocBuf.u32CurLen += (4 - (pstSender->stLocBuf.u32CurLen%4));
            }
            s32Ret = HI_MPI_VENC_ReleaseStream(vencChn, &stVStream);
            HI_ASSERT((HI_SUCCESS == s32Ret));

            free(stVStream.pstPack);
            stVStream.pstPack = NULL;
        }

        /* while writed sufficient stream frame or buffer is full, send local data to pci target */
        if ((s32StreamCnt >= PCIV_FRMNUM_ONCEDMA)|| (bBufFull == HI_TRUE))
        {
            int times = 0;
            while (PCIV_Trans_SendData(pCreator) && pstCtx->bThreadStart)
            {
                usleep(0);
                /* 发送失败次数越多，说明主片接收数据越不及时 */
                if (++times >=1) printf("PCIV_Trans_SendData, times:%d\n", times);
            }
            s32StreamCnt = 0;/* reset stream count after send stream to remote chip successed */
        }
        usleep(0);
    }

    pstCtx->bThreadStart = HI_FALSE;
    return NULL;
}


HI_S32 SamplePciv_HostSendVenc(HI_S32 s32RmtChip, HI_U32 u32GrpCnt,
    PIC_SIZE_E aenPicSize[2], PAYLOAD_TYPE_E aenType[2], HI_BOOL bHaveMinor)
{
    HI_S32 s32Ret;
    HI_U32 OffsetAddr = 0;
    SAMPLE_PCIV_MSG_S  stMsg;
    PCIV_PCIVCMD_MALLOC_S stMallocCmd;
    PCIV_PCIVCMD_MALLOC_S *pstMallocEcho;
    PCIV_TRANS_ATTR_S *pstStreamArgs;
    PCIV_VENCCMD_INIT_S stVencMsg;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stPcivVencCtx;

    if (!u32GrpCnt)
    {
        return HI_SUCCESS;
    }

    /* send msg to slave(PCI Device), for malloc Dest Addr of stream buffer */
    /* PCI transfer data from Host to Device, Dest Addr must in PCI WINDOW of PCI Device */
    stMallocCmd.u32BlkCount = 1;
    stMallocCmd.u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    memcpy(stMsg.cMsgBody, &stMallocCmd, sizeof(PCIV_PCIVCMD_MALLOC_S));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_MALLOC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_MALLOC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* read msg, phyaddr for DMA will return */
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    pstMallocEcho = (PCIV_PCIVCMD_MALLOC_S *)stMsg.cMsgBody;
    printf("func:%s, phyaddr:0x%x\n", __FUNCTION__, pstMallocEcho->u32PhyAddr[0]);
    printf("\nMalloc remote buf succed! Address is 0x%x.\n", pstMallocEcho->u32PhyAddr[0]);
    OffsetAddr = pstMallocEcho->u32PhyAddr[0];
    /* create group and venc chn */
    s32Ret = SAMPLE_StartVenc(u32GrpCnt, bHaveMinor, aenType, aenPicSize);
    PCIV_CHECK_ERR(s32Ret);

    /* config venc chn info, send message to slave to create venc stream receive thread */
    stVencMsg.u32GrpCnt =  u32GrpCnt;
    stVencMsg.bHaveMinor = bHaveMinor;
    stVencMsg.aenSize[0] = aenPicSize[0];
    stVencMsg.aenSize[1] = aenPicSize[1];
    stVencMsg.aenType[0] = PT_H264;
    stVencMsg.aenType[1] = PT_H264;

    //pstStreamArgs = &stVencMsg.stStreamArgs;
    pstStreamArgs = &stVencMsg.stStreamArgs;
    pstStreamArgs->u32BufSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    pstStreamArgs->u32PhyAddr = pstMallocEcho->u32PhyAddr[0];
    pstStreamArgs->s32RmtChip = 0;

    /* notes:msg port have open when init */
    pstStreamArgs->s32MsgPortWrite = pstVencCtx->s32MsgPortWrite;
    pstStreamArgs->s32MsgPortRead  = pstVencCtx->s32MsgPortRead;

    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_HOST_START_VENC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_VENCCMD_INIT_S);
    memcpy(stMsg.cMsgBody, &stVencMsg, sizeof(PCIV_VENCCMD_INIT_S));
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_HOST_START_VENC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* read msg */
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* init stream creater  */
    pstStreamArgs->s32RmtChip = s32RmtChip;
    pstStreamArgs->u32PhyAddr = OffsetAddr + g_u32PfWinBase[s32RmtChip];
    printf("\nOffsetAddr is 0x%x.\n", OffsetAddr);
    printf("======pstStreamArgs->u32PhyAddr is 0x%x, PFBaseAddr is 0x%x\n", pstStreamArgs->u32PhyAddr, g_u32PfWinBase[s32RmtChip]);
    s32Ret = PCIV_Trans_InitSender(pstStreamArgs, &pstVencCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    /* 使操作系统支持非4字节对齐内存操作 */
    system("echo 2 > /proc/cpu/alignment");

    pstVencCtx->s32VencCnt = (bHaveMinor) ? (u32GrpCnt*2) : u32GrpCnt;
    pstVencCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVencCtx->pid, NULL, SamplePciv_HostSendVencThread, pstVencCtx);

    printf("venc init ok, grp cnt :%d, size:%d================================\n", u32GrpCnt,aenPicSize[0]);
    return HI_SUCCESS;

}

HI_S32 SamplePciv_HostToSlaveVenc(HI_S32 s32ViChnCnt, HI_S32 s32RmtChip, HI_U32 u32GrpCnt,
                     PIC_SIZE_E aenPicSize [2], PAYLOAD_TYPE_E aenType [2], HI_BOOL bHaveMinor)
{
    HI_S32 s32Ret;
    s32Ret = SAMPLE_StartVi_SD(s32ViChnCnt, VI_DEV_BT656_D1_4MUX, aenPicSize[0]);
    if (s32Ret != HI_SUCCESS)
    {
        printf("SAMPLE_StartVi_SD failed.====================\n");
        return s32Ret;
    }

    s32Ret = SamplePciv_HostInitWinVb(s32RmtChip);
    if (s32Ret != HI_SUCCESS)
    {
        printf("SamplePciv_HostInitWinVb failed.====================\n");
        return s32Ret;
    }

    s32Ret = SamplePciv_HostSendVenc(s32RmtChip, u32GrpCnt, aenPicSize, aenType, HI_TRUE);
    if (s32Ret != HI_SUCCESS)
    {
        printf("SamplePciv_HostSendVenc failed.====================\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivInitMsgPort(HI_S32 s32RmtChip)
{
    HI_S32 i, s32Ret;
    HI_S32 s32MsgPort = PCIV_MSG_BASE_PORT+1;

    SAMPLE_PCIV_MSG_S stMsg;
    PCIV_MSGPORT_INIT_S stMsgPort;

    /* all venc stream use one pci transfer port */
    g_stPcivVencCtx.s32MsgPortWrite = s32MsgPort++;
    g_stPcivVencCtx.s32MsgPortRead  = s32MsgPort++;
    s32Ret = PCIV_OpenMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortWrite);
    s32Ret += PCIV_OpenMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortRead);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    /* each vdec stream use one pci transfer port */
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        astPcivVdecCtx[i].s32MsgPortWrite   = s32MsgPort++;
        astPcivVdecCtx[i].s32MsgPortRead    = s32MsgPort++;
        s32Ret = PCIV_OpenMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortWrite);
        s32Ret += PCIV_OpenMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortRead);
        HI_ASSERT(HI_SUCCESS == s32Ret);
    }

    /* send msg port to pci slave device --------------------------------------------------*/
    stMsgPort.s32VencMsgPortW = g_stPcivVencCtx.s32MsgPortWrite;
    stMsgPort.s32VencMsgPortR = g_stPcivVencCtx.s32MsgPortRead;
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        stMsgPort.s32VdecMsgPortW[i] = astPcivVdecCtx[i].s32MsgPortWrite;
        stMsgPort.s32VdecMsgPortR[i] = astPcivVdecCtx[i].s32MsgPortRead;
    }
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_MSG_PORG;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_MSGPORT_INIT_S);
    memcpy(stMsg.cMsgBody, &stMsgPort, sizeof(PCIV_MSGPORT_INIT_S));
    printf("\n============Prepare to open venc and vdec ports!=======\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_VOID SamplePcivExitMsgPort(HI_S32 s32RmtChip)
{
    HI_S32 i;
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    /* close all stream msg port in local */
    PCIV_CloseMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortWrite);
    PCIV_CloseMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortRead);
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        PCIV_CloseMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortWrite);
        PCIV_CloseMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortRead);
    }

    /* close all stream msg port in remote */
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_MSG_PORG;
    stMsg.stMsgHead.u32MsgLen = 0;
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_MSG_PORG==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(0);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);
}

HI_S32 SamplePcivInitMpp()
{
    VB_CONF_S stVbConf = {0};

    stVbConf.astCommPool[0].u32BlkSize = 768 * 576 * 2;/*D1*/
    stVbConf.astCommPool[0].u32BlkCnt  = 64;
    stVbConf.astCommPool[1].u32BlkSize = 384 * 288 * 2;/*1/2 D1*/
    stVbConf.astCommPool[1].u32BlkCnt  = 40;
    stVbConf.astCommPool[2].u32BlkSize = 256 * 192 * 2;/*1/3 D1*/
    stVbConf.astCommPool[2].u32BlkCnt  = 40;
    stVbConf.astCommPool[3].u32BlkSize = 192 * 144 * 2;/*1/4 D1*/
    stVbConf.astCommPool[3].u32BlkCnt  = 40;
    stVbConf.astCommPool[4].u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    stVbConf.astCommPool[4].u32BlkCnt  = 1;

    stVbConf.astCommPool[5].u32BlkSize = 768 * 576 * 2;/*D1*/
    stVbConf.astCommPool[5].u32BlkCnt  = 64;
    strcpy(stVbConf.astCommPool[5].acMmzName, "ddr1");
    stVbConf.astCommPool[6].u32BlkSize = 384 * 288 * 2;/*1/2 D1*/
    stVbConf.astCommPool[6].u32BlkCnt  = 40;
    strcpy(stVbConf.astCommPool[6].acMmzName, "ddr1");
    stVbConf.astCommPool[7].u32BlkSize = 256 * 192 * 2;/*1/3 D1*/
    stVbConf.astCommPool[7].u32BlkCnt  = 40;
    strcpy(stVbConf.astCommPool[7].acMmzName, "ddr1");
    stVbConf.astCommPool[8].u32BlkSize = 192 * 144 * 2;/*1/4 D1*/
    stVbConf.astCommPool[8].u32BlkCnt  = 40;
    strcpy(stVbConf.astCommPool[8].acMmzName, "ddr1");
    stVbConf.astCommPool[9].u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    stVbConf.astCommPool[9].u32BlkCnt  = 1;
    strcpy(stVbConf.astCommPool[9].acMmzName, "ddr1");

    stVbConf.astCommPool[10].u32BlkSize = 1920 * 1080 * 2;/*1080p*/
    stVbConf.astCommPool[10].u32BlkCnt  = 10;
    stVbConf.astCommPool[11].u32BlkSize = 1920 * 1080 * 2;/*1080p*/
    stVbConf.astCommPool[11].u32BlkCnt  = 10;
    strcpy(stVbConf.astCommPool[11].acMmzName, "ddr1");

    return SAMPLE_InitMPP(&stVbConf);
}

HI_S32 SamplePcivInitComm(HI_S32 s32RmtChipId)
{
    HI_S32 s32Ret;

    /* wait for pci device connect ... ...  */
    s32Ret = PCIV_WaitConnect(s32RmtChipId);
    PCIV_CHECK_ERR(s32Ret);
    /* open pci msg port for commom cmd */
    s32Ret = PCIV_OpenMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
    PCIV_CHECK_ERR(s32Ret);

    /* open pci msg port for all stream transfer(venc stream and vdec stream) */
    s32Ret = SamplePcivInitMsgPort(s32RmtChipId);
    PCIV_CHECK_ERR(s32Ret);

    return HI_SUCCESS;
}

int SamplePcivEnumChip(int *local_id,int remote_id[HISI_MAX_MAP_DEV-1], int *count)
{
    int fd, i;
    struct hi_mcc_handle_attr attr;

    fd = open("/dev/mcc_userdev", O_RDWR);
    if (fd<=0)
    {
        printf("open mcc dev fail\n");
        return -1;
    }

    /* HI_MCC_IOC_ATTR_INIT should be sent first ! */
    if (ioctl(fd, HI_MCC_IOC_ATTR_INIT, &attr))
    {
	    printf("initialization for attr failed!\n");
	    return -1;
    }

    *local_id = ioctl(fd, HI_MCC_IOC_GET_LOCAL_ID, &attr);
    printf("pci local id is %d \n", *local_id);

    if (ioctl(fd, HI_MCC_IOC_GET_REMOTE_ID, &attr))
    {
        printf("get pci remote id fail \n");
        return -1;
    }
    for (i=0; i<HISI_MAX_MAP_DEV-1; i++)
    {
        if (-1 == attr.remote_id[i]) break;
        *(remote_id++) = attr.remote_id[i];
        printf("get pci remote id : %d \n", attr.remote_id[i]);
    }

    *count = i;

    printf("===================close port %d!\n", attr.port);
    close(fd);
    return 0;
}

HI_S32 SamplePcivGetPfWin(HI_S32 s32ChipId)
{
    HI_S32 s32Ret;
    PCIV_BASEWINDOW_S stPciBaseWindow;
    s32Ret = HI_MPI_PCIV_GetBaseWindow(s32ChipId, &stPciBaseWindow);
    PCIV_CHECK_ERR(s32Ret);
    printf("pci device %d -> slot:%d, pf:%x,np:%x,cfg:%x\n", s32ChipId, s32ChipId-1,
        stPciBaseWindow.u32PfWinBase,stPciBaseWindow.u32NpWinBase,stPciBaseWindow.u32CfgWinBase);
    g_u32PfWinBase[s32ChipId] = stPciBaseWindow.u32PfWinBase;
    return HI_SUCCESS;
}

int SamplePcivSwitchPic(HI_U32 u32RmtIdx, HI_S32 s32RemoteChip, VO_DEV VoDev)
{
    char ch;
    HI_S32 i, k=0, s32Ret, u32DispIdx, s32VoPicDiv;
    HI_U32 u32Width, u32Height;
#if TEST_OTHER_DIVISION    
    HI_S32 as32VoPicDiv[] = {1, 4, 9, 16, 76, 100, 114, 132, 174, 200, 242, 300, 320, 640};
#else
    HI_S32 as32VoPicDiv[] = {1, 4, 9, 16};
#endif    
    SAMPLE_PCIV_DISP_CTX_S *pstDispCtx = NULL;

    /* find display contex by vo dev */
    for (u32DispIdx=0; u32DispIdx<VO_MAX_DEV_NUM; u32DispIdx++)
    {
        if (VoDev == s_astPcivDisp[u32DispIdx].VoDev)
        {
            pstDispCtx = &s_astPcivDisp[u32DispIdx];
            break;
        }
    }
    if (NULL == pstDispCtx) return HI_FAILURE;

    s32VoPicDiv = pstDispCtx->u32PicDiv;

    while (1)
    {
        printf("\n >>>> Please input commond as follow : \n");
        printf("\t ENTER : switch display div , 1 -> 4-> 9 -> 16 -> 1 -> ... \n");
        printf("\t s : step play by one frame \n");
        printf("\t q : quit the pciv sample \n");
        printf("------------------------------------------------------------------\n");

        ch = getchar();
        if (ch == 'q')
            break;
        else if ((1 == vdec_idx) && ('s' == ch))
        {
            s32Ret = HI_MPI_VO_ChnStep(VoDev, 0);
            PCIV_CHECK_ERR(s32Ret);
        }
        else
        {
            /* 1, disable all vo chn */
            for (i=0; i<s32VoPicDiv; i++)
            {
                HI_MPI_VO_DisableChn(VoDev, i);
            }

            /* 2, stop all pciv chn */
            SamplePciv_StopPcivByVo(u32RmtIdx, s32RemoteChip, u32DispIdx, pstDispCtx);

            /* switch pic 1 -> 4-> 9 -> 16 -> 1 ->... */
            s32VoPicDiv = as32VoPicDiv[(k++)%(sizeof(as32VoPicDiv)/sizeof(HI_S32))];

            /* 3, restart pciv chn by new pattern */
            pstDispCtx->u32PicDiv  = s32VoPicDiv;
            pstDispCtx->VoChnEnd   = pstDispCtx->VoChnStart + s32VoPicDiv - 1;
            s32Ret = SamplePciv_StartPcivByVo(u32RmtIdx, s32RemoteChip, u32DispIdx, pstDispCtx);
            PCIV_CHECK_ERR(s32Ret);

            /* 4, recfg vo chn and enable them by new pattern */
            u32Width  = 720;
            u32Height = 576;
            
#if TEST_OTHER_DIVISION
            switch(s32VoPicDiv)
            {
                case 76:
                case 100:
                case 114:
                case 132:    
                {
                    u32Width    = s32VoPicDiv * 4;
                    u32Height   = s32VoPicDiv * 4;
                    s32VoPicDiv = 16;
                    break;
                }
                case 174:
                {
                    u32Width    = s32VoPicDiv * 3;
                    u32Height   = s32VoPicDiv * 3;
                    s32VoPicDiv = 9;
                    break;
                }
                case 200:
                case 242:    
                {
                    u32Width    = s32VoPicDiv * 2;
                    u32Height   = s32VoPicDiv * 2;
                    s32VoPicDiv = 4;
                    break;
                }
                case 300:
                {
                    u32Width    = s32VoPicDiv;
                    u32Height   = s32VoPicDiv;
                    s32VoPicDiv = 1;
                    break;
                }    
                case 320:
                {
                    u32Width    = s32VoPicDiv;
                    u32Height   = 240;
                    s32VoPicDiv = 1;
                    break;
                }    
                case 640:    
                {
                    u32Width    = s32VoPicDiv;
                    u32Height   = 480;
                    s32VoPicDiv = 1;
                    break;
                }
                default:
                    break;
                
            }

#endif
            s32Ret = SAMPLE_SetVoChnMScreen(VoDev, s32VoPicDiv, u32Width, u32Height);
            PCIV_CHECK_ERR(s32Ret);

            for (i=0; i<s32VoPicDiv; i++)
            {
                s32Ret = HI_MPI_VO_EnableChn(VoDev, i);
                PCIV_CHECK_ERR(s32Ret);
            }
        }
    }

    return HI_SUCCESS;
}

int main(int argc, char *argv[])
{
    HI_S32 i, s32Ret, s32RmtChipId;
    HI_S32 s32ViChnCnt = VIU_MAX_CHN_NUM;
    PIC_SIZE_E enViSize = PIC_D1;
    HI_S32 s32VencGrpCnt = 16;
    PIC_SIZE_E aenVencSize[2] = {PIC_D1, PIC_CIF};
    HI_U32 u32VdecCnt = 16;
    HI_S32 s32PciRmtChipCnt = 1;   /* only test one slave dev */
    HI_S32 s32PciLocalId, as32PciRmtId[PCIV_MAX_CHIPNUM], s32AllPciRmtCnt;

    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN_ATTR_S astChnAttr_16[16];
    VO_DEV VoDev = 2;
    HI_U32 u32VoChnNum = 16;

    HI_U32 u32VpssCnt = 16;

    if (argc > 1)
    {
        switch (*argv[1])
        {
            default:
            case '0':/* VI->PCIV->PCIV->VO(SD) */
                test_idx = 0;
                Add_osd  = 0;
                break;
            case '1':/* VI->PCIV->PCIV->VO(SD) and add osd to pciture before dma*/
                test_idx = 0;
                Add_osd  = 1;
                break;   
            case '2':/* VI->VPSS->PCIV->PCIV->VO(SD) */
                test_idx = 1;
                Add_osd  = 0;
                break;
            case '3':/* VI->VPSS_bypass->PCIV->PCIV->VO(SD) */
                test_idx = 2;
                Add_osd  = 0;
                break;
            case '4':/* VI->VPSS_bypass->PCIV->PCIV->VO(SD) and add osd to picture before dma*/
                test_idx = 2;
                Add_osd  = 1;
                break;    
            case '5':/* VDEC->PCIV->PCIV->VO(SD) */
                test_idx = 3;
                Add_osd  = 0;
                break;    
        }
    }
    else
    {
        printf("Usage : %s [index]\n", argv[0]);
        printf("index:\n");
        printf("\t 0: VI->PCIV->PCIV->VO(SD) + VI->VENC->PCI\n");
        printf("\t 1: VI->PCIV->PCIV->VO(SD) add osd to picture before dma + VI->VENC->PCI\n");
        printf("\t 2: VI->VPSS->PCIV->PCIV->VO(SD) + VI->VPSS->VENC->PCI\n");
        printf("\t 3: VI->VPSS_bypass->PCIV->PCIV->VO(SD) + VI->VPSS->VENC->PCI\n");
        printf("\t 4: VI->VPSS_bypass->PCIV->PCIV->VO(SD) add osd to picture before dma + VI->VPSS->VENC->PCI\n");
        
        return HI_SUCCESS;
    }

    /* Get pci local id and all target id */
    s32Ret = SamplePcivEnumChip(&s32PciLocalId, as32PciRmtId, &s32AllPciRmtCnt);
    PCIV_CHECK_ERR(s32Ret);

    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        /* Wait for slave dev connect, and init message communication */
        s32Ret = SamplePcivInitComm(as32PciRmtId[i]);
        PCIV_CHECK_ERR(s32Ret);

        /* Get PCI PF Window info of pci device, used for DMA trans that host to slave */
        (HI_VOID)SamplePcivGetPfWin(as32PciRmtId[i]);
    }

    /* Init mpp sys and video buffer */
    s32Ret = SamplePcivInitMpp();
    PCIV_CHECK_ERR(s32Ret);
    SAMPLE_COMM_SYS_MemConfig();

    /* Init vo in pci host for display  */
    SamplePciv_GetDefVoAttr(VoDev, (0 == VoDev)?VO_OUTPUT_1080P60:VO_OUTPUT_PAL, &stPubAttr, &stLayerAttr, sqrt(u32VoChnNum), astChnAttr_16);
    s32Ret = SamplePciv_StartVO(VoDev, &stPubAttr, &stLayerAttr, astChnAttr_16, u32VoChnNum, HI_FALSE);
    PCIV_CHECK_ERR(s32Ret);

#if STREAM_SEND_VDEC
    /* create host vdec/vpss/vo chn, to decode stream from slave to vo*/
    SamplePciv_HostCreateVdecVpssVo(s32VencGrpCnt, aenVencSize[0]);
#endif
    /* Init ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];

        //从->主  Host receive venc stream
        /* init vi in slave chip */
        s32Ret = SamplePciv_HostInitVi(s32RmtChipId, s32ViChnCnt, enViSize);
        PCIV_CHECK_ERR(s32Ret);

        /* Start vpss by chip */
        if (1 == test_idx || 2 == test_idx)
        {
            s32Ret = SamplePciv_StartVpssByChip(s32RmtChipId, u32VpssCnt, 0);
            PCIV_CHECK_ERR(s32Ret);
        }

        /* init vdec if you select play mode */
        if (3 == test_idx)
        {
            s32Ret = SamplePciv_StartVdecByChip(s32RmtChipId, u32VdecCnt);
            PCIV_CHECK_ERR(s32Ret);
        }

        /* Start pciv chn by chip */
        s32Ret = SamplePciv_StartPcivByChip(i, s32RmtChipId);
        PCIV_CHECK_ERR(s32Ret);
#if PCIV_START_STREAM
        /* Init venc and stream transfer (venc is in slave and send stream to host) */
        s32Ret = SamplePciv_HostStartVenc(s32RmtChipId, s32VencGrpCnt, aenVencSize, HI_FALSE);
        PCIV_CHECK_ERR(s32Ret);
#endif
#if 0   //Host send venc stream to slave
        s32Ret = SamplePciv_HostToSlaveVenc(s32ViChnCnt, s32RmtChipId, s32VencGrpCnt, aenVencSize, aenType, HI_TRUE);
        PCIV_CHECK_ERR(s32Ret);
#endif
    }

    /* DEMO: switch pic div , only demo the first slave dev ------------------------------ */
    SamplePcivSwitchPic(0, as32PciRmtId[0], VoDev);

    printf("\n++++++++++++++++++++++++++++++++++++++++++Host system exit!\n");

    /* Exit ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];
#if PCIV_START_STREAM
        /* stop all venc chn */
        SamplePciv_HostStopVenc(s32RmtChipId, s32VencGrpCnt, HI_TRUE);
#endif
        /* stop all pciv chn by dev */
        SamplePciv_StopPcivByChip(s32RmtChipId, i);

        /* stop all vpss by dev */
        if (1 == test_idx || 2 == test_idx)
        {
            SamplePciv_StopVpssByChip(s32RmtChipId, u32VpssCnt, 0);
        }

        /* stop vdec in host and slave*/
        if (1 == vdec_idx)
        {
            SamplePciv_StopVdecByChip(s32RmtChipId, u32VdecCnt);
        }

        /* stop vi in slave */
        SamplePciv_HostExitVi(s32RmtChipId, VIU_MAX_CHN_NUM);

        /* close all msg port */
        SamplePcivExitMsgPort(s32RmtChipId);
        PCIV_CloseMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
    }
#if STREAM_SEND_VDEC
    SamplePciv_HostDestroyVdecVpssVo(s32VencGrpCnt);
#endif
    SamplePciv_StopVO(VoDev, u32VoChnNum, HI_FALSE);

    /* Exit whole mpp sys  */
    SAMPLE_ExitMPP();
    return HI_SUCCESS;
}



