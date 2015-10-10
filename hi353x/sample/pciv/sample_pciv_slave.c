/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_pciv_slave.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/09/22
  Description   : this sample of pciv in PCI device
  History       :
  1.Date        : 2009/09/22
    Author      : Hi3520MPP
    Modification: Created file
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: 将消息端口的打开操作放到最开始的初始化过程中
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
#include "hi_debug.h"

#include "hi_comm_pciv.h"
#include "mpi_pciv.h"
#include "pciv_msg.h"
#include "pciv_trans.h"
#include "sample_pciv_comm.h"
#include "sample_common.h"
#include "loadbmp.h"


#define PCIV_FRMNUM_ONCEDMA 5

typedef struct hiSAMPLE_PCIV_CTX_S
{
    VDEC_CHN VdChn;
    pthread_t pid;
    HI_BOOL bThreadStart;
    HI_CHAR aszFileName[64];
} SAMPLE_PCIV_CTX_S;

static SAMPLE_PCIV_CTX_S g_astSamplePciv[PCIV_MAX_CHN_NUM];
static SAMPLE_PCIV_VENC_CTX_S g_stSamplePcivVenc = {0};
static SAMPLE_PCIV_VDEC_CTX_S g_astSamplePcivVdec[VDEC_MAX_CHN_NUM] = {{0}};

static HI_S32 g_s32PciLocalId  = -1;
static HI_U32 g_u32PfAhbBase   = 0;
static HI_S32 PCIV_ADD_OSD     = 0;

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

HI_S32 SamplePcivStopVdec(VDEC_CHN VdChn)
{
    SAMPLE_PCIV_CTX_S *pstCtx;

    HI_MPI_VDEC_StopRecvStream(VdChn);
    HI_MPI_VDEC_DestroyChn(VdChn);

    pstCtx = &g_astSamplePciv[VdChn];
    if (pstCtx->bThreadStart == HI_TRUE)
    {
        printf("start pthread_join vdec %d \n", VdChn);
        pstCtx->bThreadStart = HI_FALSE;
        pthread_join(pstCtx->pid, 0);
    }

    printf("vdec chn %d destroyed ok\n", VdChn);

    return HI_SUCCESS;
}

HI_S32 SamplePcivStartVdec(VDEC_CHN VdChn, PIC_SIZE_E enPicSize)
{
    HI_S32 s32ret;
    VDEC_CHN_ATTR_S stAttr;

    if (PIC_D1 == enPicSize)
    {
        stAttr.u32PicWidth = 704;
        stAttr.u32PicHeight = 576;
    }
    else if (PIC_CIF == enPicSize)
    {
        stAttr.u32PicWidth = 384;
        stAttr.u32PicHeight = 288;
    }
    else
    {
        printf("not support vdec size %d \n", enPicSize);
        return HI_FAILURE;
    }

    stAttr.enType = PT_H264;
    stAttr.u32BufSize = ((stAttr.u32PicWidth * stAttr.u32PicHeight) << 1);
    stAttr.stVdecVideoAttr.enMode = VIDEO_MODE_FRAME;
    stAttr.stVdecVideoAttr.s32SupportBFrame = 0;
    stAttr.stVdecVideoAttr.u32RefFrameNum   = 1;
    
    s32ret = HI_MPI_VDEC_CreateChn(VdChn, &stAttr);  
    if (HI_SUCCESS != s32ret)
    {
        printf("HI_MPI_VDEC_CreateChn %d failed, errno 0x%x \n", VdChn, s32ret);
        return s32ret;
    }

    s32ret = HI_MPI_VDEC_StartRecvStream(VdChn);
    if (HI_SUCCESS != s32ret)
    {
        printf("HI_MPI_VDEC_StartRecvStream %d failed, errno 0x%x \n", VdChn, s32ret);
        return s32ret;
    }
    printf("create vdec chn %d ok\n", VdChn);

    return HI_SUCCESS;
}

HI_S32 SamplePcivEchoMsg(HI_S32 s32RetVal, HI_S32 s32EchoMsgLen, SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;

    pMsg->stMsgHead.u32Target   = 0; /* To host */
    pMsg->stMsgHead.s32RetVal   = s32RetVal;
    pMsg->stMsgHead.u32MsgType   = SAMPLE_PCIV_MSG_ECHO;
    pMsg->stMsgHead.u32MsgLen   = s32EchoMsgLen + sizeof(SAMPLE_PCIV_MSGHEAD_S);
    s32Ret = PCIV_SendMsg(0, PCIV_MSGPORT_COMM_CMD, pMsg);
    HI_ASSERT(s32Ret != HI_FAILURE);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveInitWinVb(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_WINVB_S *pstWinVbArgs = (SAMPLE_PCIV_MSG_WINVB_S*)pMsg->cMsgBody;

    /* create buffer pool in PCI Window */
    s32Ret = HI_MPI_PCIV_WinVbDestroy();
    s32Ret = HI_MPI_PCIV_WinVbCreate(&pstWinVbArgs->stPciWinVbCfg);
    PCIV_CHECK_ERR(s32Ret);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveMalloc(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret = HI_SUCCESS, i;
    HI_U32 au32PhyAddr[PCIV_MAX_BUF_NUM];
    PCIV_PCIVCMD_MALLOC_S *pstMallocArgs = (PCIV_PCIVCMD_MALLOC_S *)pMsg->cMsgBody;

    /* in slave chip, this func will alloc a buffer from Window MMZ */
    s32Ret = HI_MPI_PCIV_Malloc(pstMallocArgs->u32BlkSize, pstMallocArgs->u32BlkCount, au32PhyAddr);
    HI_ASSERT(!s32Ret);

    /* Attation: return the offset from PCI shm_phys_addr */
    for(i=0; i<pstMallocArgs->u32BlkCount; i++)
    {
        pstMallocArgs->u32PhyAddr[i] = au32PhyAddr[i] - g_u32PfAhbBase;
        printf("func:%s, phyaddr:0x%x = 0x%x - 0x%x \n",
            __FUNCTION__, pstMallocArgs->u32PhyAddr[i], au32PhyAddr[i], g_u32PfAhbBase);
    }

    return HI_SUCCESS;
}

void* SamplePcivVdStreamThread(void* arg)
{
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = (SAMPLE_PCIV_VDEC_CTX_S*)arg;
    HI_VOID *pReceiver = pstVdecCtx->pTransHandle;
    PCIV_STREAM_HEAD_S *pStrmHead = NULL;
    HI_U8 *pu8Addr;
    HI_U32 u32Len;
    VDEC_STREAM_S stStream;

    while (pstVdecCtx->bThreadStart)
    {
        /* get data from pciv stream receiver */
        if (PCIV_Trans_GetData(pReceiver, &pu8Addr, &u32Len))
        {
            usleep(0);
            continue;
        }

        pStrmHead = (PCIV_STREAM_HEAD_S *)pu8Addr;
        HI_ASSERT(PCIV_STREAM_MAGIC == pStrmHead->u32Magic);
        HI_ASSERT(u32Len >= pStrmHead->u32DMADataLen + sizeof(PCIV_STREAM_HEAD_S));

        /* send the data to video decoder */
        stStream.pu8Addr = pu8Addr + sizeof(PCIV_STREAM_HEAD_S);
        stStream.u64PTS = 0;
        while (HI_TRUE == pstVdecCtx->bThreadStart &&
            HI_MPI_VDEC_SendStream(pstVdecCtx->VdecChn, &stStream, HI_IO_NOBLOCK))
        {
            usleep(0);
        }

        memset(pu8Addr, 0, u32Len);
        /* release data to pciv stream receiver */
        PCIV_Trans_ReleaseData(pReceiver, pu8Addr, u32Len);
    }

    pstVdecCtx->bThreadStart = HI_FALSE;
    return NULL;
}


HI_S32 SamplePciv_SlaveStartVdecStream(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_TRANS_ATTR_S *pstTransAttr = (PCIV_TRANS_ATTR_S*)pMsg->cMsgBody;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &g_astSamplePcivVdec[pstTransAttr->s32ChnId];

    /* msg port should have open when SamplePciv_SlaveInitPort() */
    HI_ASSERT(pstTransAttr->s32MsgPortWrite == pstVdecCtx->s32MsgPortWrite);
    HI_ASSERT(pstTransAttr->s32MsgPortRead  == pstVdecCtx->s32MsgPortRead);

    /* init vdec stream receiver */
    pstTransAttr->u32PhyAddr += g_u32PfAhbBase;/* NOTE:phyaddr in msg is a offset */
    s32Ret = PCIV_Trans_InitReceiver(pstTransAttr, &pstVdecCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    pstVdecCtx->bThreadStart = HI_TRUE;
    pstVdecCtx->VdecChn = pstTransAttr->s32ChnId;
    /* create thread to get stream coming from host chip, and send stream to decoder */
    pthread_create(&pstVdecCtx->pid, NULL, SamplePcivVdStreamThread, pstVdecCtx);

    printf("init vdec:%d stream receiver in slave chip ok!\n", pstTransAttr->s32ChnId);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVdecStream(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_TRANS_ATTR_S *pstInitCmd = (PCIV_TRANS_ATTR_S*)pMsg->cMsgBody;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &g_astSamplePcivVdec[pstInitCmd->s32ChnId];

    /* exit thread*/
    if (HI_TRUE == pstVdecCtx->bThreadStart)
    {
        pstVdecCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVdecCtx->pid, 0);
    }

    /* eixt vdec stream receiver */
    s32Ret = PCIV_Trans_DeInitReceiver(pstVdecCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    printf("exit vdec:%d stream receiver in slave chip ok!\n", pstVdecCtx->VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePcivGetStreamFromVenc(VENC_CHN vencChn, VENC_STREAM_S *pStream)
{
    HI_S32          s32Ret;
	VENC_CHN_STAT_S stStat;

    s32Ret = HI_MPI_VENC_Query(vencChn, &stStat);
    if ((s32Ret != HI_SUCCESS) || (stStat.u32CurPacks == 0))
    {
        return HI_FAILURE;
    }

    pStream->u32PackCount = stStat.u32CurPacks;
    if (pStream->u32PackCount != 1 && pStream->u32PackCount != 4)
    {
        printf("pStream->u32PackCount:%d, venc:%d, packs:%d,leftpic:%d,leftstrm:%d \n",
            pStream->u32PackCount, vencChn, stStat.u32CurPacks,
            stStat.u32LeftPics, stStat.u32LeftStreamBytes);
        HI_ASSERT(0);
    }


    s32Ret = HI_MPI_VENC_GetStream(vencChn, pStream, HI_IO_NOBLOCK);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_GetStream err, ret:%x, pack:%d\n", s32Ret, stStat.u32CurPacks);
        pStream->u32PackCount = 0;
        HI_ASSERT(0);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}


/* get stream from venc chn, write to local buffer, then send to pci target,
    we send several stream frame one time, to improve PCI DMA efficiency  */
HI_VOID * SamplePciv_SendVencThread(HI_VOID *p)
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

HI_S32 SamplePciv_SlaveStartVenc(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret,i;
    PCIV_VENCCMD_INIT_S *pstMsgCreate = (PCIV_VENCCMD_INIT_S *)pMsg->cMsgBody;
    PCIV_TRANS_ATTR_S *pstStreamArgs = &pstMsgCreate->stStreamArgs;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stSamplePcivVenc;
    MPP_CHN_S stBindSrc;
    MPP_CHN_S stBindDest;

    /* Bind */
    for (i=0; i<pstMsgCreate->u32GrpCnt; i++)
    {
        if (pstMsgCreate->bUseVpss)
        {
            stBindSrc.enModId = HI_ID_VPSS;
            stBindSrc.s32DevId = i;
            stBindSrc.s32ChnId = VPSS_BSTR_CHN;
        }
        else
        {
            stBindSrc.enModId = HI_ID_VIU;
            stBindSrc.s32DevId = 0;
            stBindSrc.s32ChnId = i;
        }

        stBindDest.enModId = HI_ID_GROUP;
        stBindDest.s32DevId = i;
        stBindDest.s32ChnId = 0;

        s32Ret = HI_MPI_SYS_Bind(&stBindSrc, &stBindDest);
        if (s32Ret != HI_SUCCESS)
        {
            printf("vi bind venc err 0x%x\n", s32Ret);
            return HI_FAILURE;
        }
    }

    /* create group and venc chn */
    s32Ret = SAMPLE_StartVenc(pstMsgCreate->u32GrpCnt, pstMsgCreate->bHaveMinor,
        pstMsgCreate->aenType, pstMsgCreate->aenSize);
    PCIV_CHECK_ERR(s32Ret);


    /* msg port should have open when SamplePciv_SlaveInitPort() */
    HI_ASSERT(pstStreamArgs->s32MsgPortWrite == pstVencCtx->s32MsgPortWrite);
    HI_ASSERT(pstStreamArgs->s32MsgPortRead == pstVencCtx->s32MsgPortRead);

    /* init stream creater  */
    pstStreamArgs->s32RmtChip = 0;
    s32Ret = PCIV_Trans_InitSender(pstStreamArgs, &pstVencCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    pstVencCtx->s32VencCnt = (pstMsgCreate->bHaveMinor) ? \
                        (pstMsgCreate->u32GrpCnt*2) : pstMsgCreate->u32GrpCnt;

    /* 使操作系统支持非4字节对齐内存操作 */
    system("echo 2 > /proc/cpu/alignment");

    pstVencCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVencCtx->pid, NULL, SamplePciv_SendVencThread, pstVencCtx);

    printf("venc init ok, grp cnt :%d, size:%d================================\n",
        pstMsgCreate->u32GrpCnt,pstMsgCreate->aenSize[0]);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVenc(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 i,s32Ret;
    PCIV_VENCCMD_INIT_S *pstMsgCreate = (PCIV_VENCCMD_INIT_S *)pMsg->cMsgBody;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stSamplePcivVenc;
    MPP_CHN_S stBindSrc;
    MPP_CHN_S stBindDest;

    /* exit the thread of sending stream */
    if (pstVencCtx->bThreadStart)
    {
        pstVencCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVencCtx->pid, 0);
    }

    /* destroy all venc chn */
    s32Ret = SAMPLE_StopVenc(pstMsgCreate->u32GrpCnt, pstMsgCreate->bHaveMinor);
    PCIV_CHECK_ERR(s32Ret);

    /* Unbind */
    for (i=0; i<pstMsgCreate->u32GrpCnt; i++)
    {
        if (pstMsgCreate->bUseVpss)
        {
            stBindSrc.enModId = HI_ID_VPSS;
            stBindSrc.s32DevId = i;
            stBindSrc.s32ChnId = VPSS_BSTR_CHN;
        }
        else
        {
            stBindSrc.enModId = HI_ID_VIU;
            stBindSrc.s32DevId = 0;
            stBindSrc.s32ChnId = 0;
        }

        stBindDest.enModId = HI_ID_GROUP;
        stBindDest.s32DevId = i;
        stBindDest.s32ChnId = 0;

        s32Ret = HI_MPI_SYS_UnBind(&stBindSrc, &stBindDest);
        if (s32Ret != HI_SUCCESS)
        {
            printf("vi bind venc err 0x%x\n", s32Ret);
            return HI_FAILURE;
        }
    }

    /* exit the transfer sender */
    PCIV_Trans_DeInitSender(pstVencCtx->pTransHandle);

    printf("venc exit ok, grp cnt :%d===========================\n",pstMsgCreate->u32GrpCnt);
    return HI_SUCCESS;
}


HI_VOID *SamplePciv_SlaveRevVeStrmThread(HI_VOID *arg)
{
    SAMPLE_PCIV_VENC_CTX_S *pstCtx = (SAMPLE_PCIV_VENC_CTX_S*)arg;
    HI_VOID *pReceiver = pstCtx->pTransHandle;
    PCIV_STREAM_HEAD_S *pStrmHead = NULL;
    HI_U8 *pu8Addr, *pu8AddrTmp;
    HI_U32 u32Len;
    HI_S32 s32VencChn;
    HI_CHAR aszFileName[64] = {0};
    HI_S32 s32WriteLen = 0;
    static FILE *pFile[VENC_MAX_CHN_NUM] = {NULL};

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

#if 1
        for (pu8AddrTmp = pu8Addr; pu8AddrTmp < (pu8Addr + u32Len); )
        {
            pStrmHead = (PCIV_STREAM_HEAD_S *)pu8AddrTmp;
            HI_ASSERT(PCIV_STREAM_MAGIC == pStrmHead->u32Magic);

            s32VencChn = pStrmHead->s32ChnID;
            HI_ASSERT(s32VencChn < VENC_MAX_CHN_NUM);

            /* save stream data to file */
            if (NULL == pFile[s32VencChn])
            {
                sprintf(aszFileName, "slave_venc_chn%d.h264", s32VencChn);
                pFile[s32VencChn] = fopen(aszFileName, "wb");
                HI_ASSERT(pFile[s32VencChn]);
            }
            s32WriteLen = fwrite(pu8AddrTmp + sizeof(PCIV_STREAM_HEAD_S), pStrmHead->u32StreamDataLen, 1, pFile[s32VencChn]);
            HI_ASSERT(1 == s32WriteLen);

            pu8AddrTmp += (sizeof(PCIV_STREAM_HEAD_S) + pStrmHead->u32DMADataLen);
            HI_ASSERT(pu8AddrTmp <= (pu8Addr + u32Len));

        }
        

        
#endif
        /* release data to pciv stream receiver */
        PCIV_Trans_ReleaseData(pReceiver, pu8Addr, u32Len);
        pstCtx->u32Seq ++;
    }

    pstCtx->bThreadStart = HI_FALSE;
    return NULL;
}


HI_S32 SamplePciv_SlaveRecvVenc(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_VENCCMD_INIT_S *pstMsgCreate = (PCIV_VENCCMD_INIT_S *)pMsg->cMsgBody;
    PCIV_TRANS_ATTR_S *pstStreamArgs = &pstMsgCreate->stStreamArgs;
    SAMPLE_PCIV_VENC_CTX_S *pstVencCtx = &g_stSamplePcivVenc;

    /* msg port should have open when SamplePciv_SlaveInitPort() */
    HI_ASSERT(pstStreamArgs->s32MsgPortWrite == pstVencCtx->s32MsgPortWrite);
    HI_ASSERT(pstStreamArgs->s32MsgPortRead == pstVencCtx->s32MsgPortRead);

    /* init stream recerver */
    pstStreamArgs->u32PhyAddr += g_u32PfAhbBase;
    s32Ret = PCIV_Trans_InitReceiver(pstStreamArgs, &pstVencCtx->pTransHandle);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* create thread to reveive venc stream from slave chip */
    pstVencCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVencCtx->pid, NULL, SamplePciv_SlaveRevVeStrmThread, pstVencCtx);

    return HI_SUCCESS;

}
HI_S32 SamplePciv_SlaveStartVi(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_INIT_VI_S *pstViArg = (SAMPLE_PCIV_MSG_INIT_VI_S*)pMsg->cMsgBody;

    s32Ret = SAMPLE_StartVi_SD(pstViArg->s32ViChnCnt, pstViArg->enViDevType, pstViArg->enPicSize);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVi(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;

    s32Ret = SAMPLE_StopAllVi();
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    printf("stop all vi ok!\n");
    return HI_SUCCESS;
}

HI_S32 SamplePcivLoadRgnBmp(const char *filename, BITMAP_S *pstBitmap, HI_BOOL bFil, HI_U32 u16FilColor)
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
    
    pstBitmap->pData = malloc(2*(bmpInfo.bmiHeader.biWidth)*(bmpInfo.bmiHeader.biHeight));
	
    if(NULL == pstBitmap->pData)
    {
        printf("malloc osd memroy err!\n");        
        return HI_FAILURE;
    }
    CreateSurfaceByBitMap(filename,&Surface,(HI_U8*)(pstBitmap->pData));
	
    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_1555;

    int i,j;
    HI_U16 *pu16Temp;
    pu16Temp = (HI_U16*)pstBitmap->pData;
    
    if (bFil)
    {
        for (i=0; i<pstBitmap->u32Height; i++)
        {
            for (j=0; j<pstBitmap->u32Width; j++)
            {
                if (u16FilColor == *pu16Temp)
                {
                    *pu16Temp &= 0x7FFF;
                }

                pu16Temp++;
            }
        }

    }
        
    return HI_SUCCESS;
}


HI_S32 SamplePcivChnCreateRegion(PCIV_CHN PcivChn)
{
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stChnAttr;
    BITMAP_S stBitmap;
    
    /*创建区域*/
    stRgnAttr.enType = OVERLAYEX_RGN;
    stRgnAttr.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_RGB_1555;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Width  = 128;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Height = 128;
    stRgnAttr.unAttr.stOverlayEx.u32BgColor = 0xfc;
    
    s32Ret = HI_MPI_RGN_Create(PcivChn, &stRgnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region of pciv chn %d create fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
    
    /*将区域显示到通道中*/
    stChn.enModId = HI_ID_PCIV;
    stChn.s32DevId = 0;
    stChn.s32ChnId = PcivChn;
    
    stChnAttr.bShow = HI_TRUE;
    stChnAttr.enType = OVERLAYEX_RGN;
    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 128;
    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 128;
    stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha   = 255;
    stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha   = 255;
    stChnAttr.unChnAttr.stOverlayExChn.u32Layer     = 0;

    /*添加位图*/
    SamplePcivLoadRgnBmp("mm2.bmp", &stBitmap, HI_FALSE, 0);
    
    s32Ret = HI_MPI_RGN_SetBitMap(PcivChn, &stBitmap);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region set bitmap to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
    free(stBitmap.pData);
    
    s32Ret = HI_MPI_RGN_AttachToChn(PcivChn, &stChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region attach to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
       
    return HI_SUCCESS;
}

HI_S32 SamplePcivChnDestroyRegion(PCIV_CHN PcivChn)
{
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    stChn.enModId = HI_ID_PCIV;
    stChn.s32DevId = 0;
    stChn.s32ChnId = PcivChn;
    /* 区域解绑定 */
    s32Ret = HI_MPI_RGN_DetachFrmChn(PcivChn, &stChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region attach to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }

    /* 销毁区域 */
    s32Ret = HI_MPI_RGN_Destroy(PcivChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("destroy  pciv chn %d region fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }       

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStartVdec(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VDEC_S *pstVdecArgs = (SAMPLE_PCIV_MSG_VDEC_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStartVdec(pstVdecArgs->VdecChn, pstVdecArgs->enPicSize);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVdec(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VDEC_S *pstVdecArgs = (SAMPLE_PCIV_MSG_VDEC_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStopVdec(pstVdecArgs->VdecChn);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePcivStartVpss(SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs)
{
    HI_S32 i, s32Ret;
    VPSS_GRP vpssGrp = pstVpssArgs->vpssGrp;
    VPSS_GRP_ATTR_S stGrpAttr;
    MPP_CHN_S stDestChn;
    switch (pstVpssArgs->enPicSize)
    {
        case PIC_D1     :
            stGrpAttr.u32MaxW = 720;
            stGrpAttr.u32MaxH = 576;
            break;
        case PIC_HD1080 :
            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1088;
            break;
        case PIC_960H   :
            stGrpAttr.u32MaxW = 960;
            stGrpAttr.u32MaxH = 576;
            break;
        default         :
            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1088;
            break;
    }
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    stGrpAttr.bDbEn     = 0;
    stGrpAttr.bDrEn     = 0;
    stGrpAttr.bIeEn     = 0;
    stGrpAttr.bNrEn     = 1;
    stGrpAttr.bHistEn   = 1;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;

    s32Ret = HI_MPI_VPSS_CreateGrp(vpssGrp, &stGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS create group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VPSS_StartGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS start group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    stDestChn.enModId = HI_ID_VPSS;
    stDestChn.s32DevId = vpssGrp;
    stDestChn.s32ChnId = 0;
    s32Ret = HI_MPI_SYS_Bind(&pstVpssArgs->stBInd, &stDestChn);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS bind error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    for (i=0; i<VPSS_MAX_CHN_NUM; i++)
    {
        if (pstVpssArgs->vpssChnStart[i])
        {
            s32Ret = HI_MPI_VPSS_EnableChn(vpssGrp, i);
            if (HI_SUCCESS != s32Ret)
            {
                printf("VPSS enable chn error, value= 0x%x.\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    return HI_SUCCESS;
}
HI_S32 SamplePcivStopVpss(SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs)
{
    HI_S32 i, s32Ret;
    VPSS_GRP vpssGrp = pstVpssArgs->vpssGrp;
    MPP_CHN_S stDestChn;

    for (i=0; i<VPSS_MAX_CHN_NUM; i++)
    {
        if (pstVpssArgs->vpssChnStart[i])
        {
            s32Ret = HI_MPI_VPSS_DisableChn(vpssGrp, i);
            if (HI_SUCCESS != s32Ret)
            {
                printf("VPSS disable chn error, value= 0x%x.\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    s32Ret = HI_MPI_VPSS_StopGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS start group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    stDestChn.enModId = HI_ID_VPSS;
    stDestChn.s32DevId = vpssGrp;
    stDestChn.s32ChnId = 0;
    s32Ret = HI_MPI_SYS_UnBind(&pstVpssArgs->stBInd, &stDestChn);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS bind error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_DestroyGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS destroy group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStartVpss(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs = (SAMPLE_PCIV_MSG_VPSS_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStartVpss(pstVpssArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}
HI_S32 SamplePciv_SlaveStopVpss(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs = (SAMPLE_PCIV_MSG_VPSS_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStopVpss(pstVpssArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStartPciv(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_PCIVCMD_CREATE_S *pstMsgCreate = (PCIV_PCIVCMD_CREATE_S *)pMsg->cMsgBody;
    PCIV_CHN PcivChn = pstMsgCreate->pcivChn;
    PCIV_ATTR_S *pstPicvAttr = &pstMsgCreate->stDevAttr;
    PCIV_BIND_OBJ_S *pstBindObj = &pstMsgCreate->stBindObj[0];
    MPP_CHN_S stSrcChn,stDestChn;
    PCIV_ADD_OSD = pstMsgCreate->bAddOsd;
    
    printf("PCIV_ADD_OSD is %d.\n", PCIV_ADD_OSD);
    
    /* 1) create pciv chn */
    s32Ret = HI_MPI_PCIV_Create(PcivChn, pstPicvAttr);
    PCIV_CHECK_ERR(s32Ret);

    /* 2) bind pciv and vi or vdec */
    switch (pstBindObj->enType)
    {
        case PCIV_BIND_VI:
            stSrcChn.enModId  = HI_ID_VIU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.viDevice.viDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.viDevice.viChn;
            break;
        case PCIV_BIND_VO:
            stSrcChn.enModId  = HI_ID_VOU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.voDevice.voDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VDEC:
            stSrcChn.enModId  = HI_ID_VDEC;
            stSrcChn.s32DevId = 0;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vdecDevice.vdecChn;
            break;
        case PCIV_BIND_VPSS:
            stSrcChn.enModId  = HI_ID_VPSS;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.vpssDevice.vpssGrp;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            HI_ASSERT(0);
    }
    stDestChn.enModId  = HI_ID_PCIV;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = PcivChn;
    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
        stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
        stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
    PCIV_CHECK_ERR(s32Ret);

    /* 3) create region for pciv chn */
    if (1 == PCIV_ADD_OSD)
    {
        s32Ret = SamplePcivChnCreateRegion(PcivChn);
        if (s32Ret != HI_SUCCESS)
        {
            printf("pciv chn %d SamplePcivChnCreateRegion err, value = 0x%x. \n", PcivChn, s32Ret);
            return s32Ret;
        }
    }
    
    /* 4) start pciv chn */
    s32Ret = HI_MPI_PCIV_Start(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

    printf("slave start pciv chn %d ok, remote chn:%d=========\n",
        PcivChn,pstPicvAttr->stRemoteObj.pcivChn);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopPicv(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_PCIVCMD_DESTROY_S *pstMsgDestroy = (PCIV_PCIVCMD_DESTROY_S*)pMsg->cMsgBody;
    PCIV_CHN pcivChn = pstMsgDestroy->pcivChn;
    PCIV_BIND_OBJ_S *pstBindObj = &pstMsgDestroy->stBindObj[0];
    MPP_CHN_S stSrcChn,stDestChn;

    s32Ret = HI_MPI_PCIV_Stop(pcivChn);
    PCIV_CHECK_ERR(s32Ret);

    switch (pstBindObj->enType)
    {
        case PCIV_BIND_VI:
            stSrcChn.enModId  = HI_ID_VIU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.viDevice.viDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.viDevice.viChn;
            break;
        case PCIV_BIND_VO:
            stSrcChn.enModId  = HI_ID_VOU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.voDevice.voDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VDEC:
            stSrcChn.enModId  = HI_ID_VDEC;
            stSrcChn.s32DevId = 0;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vdecDevice.vdecChn;
            break;
        case PCIV_BIND_VPSS:
            stSrcChn.enModId  = HI_ID_VPSS;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.vpssDevice.vpssGrp;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            HI_ASSERT(0);
    }
    stDestChn.enModId  = HI_ID_PCIV;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = pcivChn;
    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
        stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
        stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
    PCIV_CHECK_ERR(s32Ret);

    if (1 == PCIV_ADD_OSD)
    {
        s32Ret = SamplePcivChnDestroyRegion(pcivChn);
        PCIV_CHECK_ERR(s32Ret);
    }
    
    s32Ret = HI_MPI_PCIV_Destroy(pcivChn);
    PCIV_CHECK_ERR(s32Ret);

    printf("pciv chn %d destroy ok \n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveInitPort(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret, i;
    PCIV_MSGPORT_INIT_S *pstMsgPort = (PCIV_MSGPORT_INIT_S*)pMsg->cMsgBody;

    g_stSamplePcivVenc.s32MsgPortWrite = pstMsgPort->s32VencMsgPortW;
    g_stSamplePcivVenc.s32MsgPortRead  = pstMsgPort->s32VencMsgPortR;
    s32Ret  = PCIV_OpenMsgPort(0, g_stSamplePcivVenc.s32MsgPortWrite);
    s32Ret |= PCIV_OpenMsgPort(0, g_stSamplePcivVenc.s32MsgPortRead);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        g_astSamplePcivVdec[i].s32MsgPortWrite  = pstMsgPort->s32VdecMsgPortW[i];
        g_astSamplePcivVdec[i].s32MsgPortRead   = pstMsgPort->s32VdecMsgPortR[i];
        s32Ret  = PCIV_OpenMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortWrite);
        s32Ret |= PCIV_OpenMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortRead);
        HI_ASSERT(HI_SUCCESS == s32Ret);
    }
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveExitPort(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 i;
    PCIV_CloseMsgPort(0, g_stSamplePcivVenc.s32MsgPortWrite);
    PCIV_CloseMsgPort(0, g_stSamplePcivVenc.s32MsgPortRead);
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        PCIV_CloseMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortWrite);
        PCIV_CloseMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortRead);
    }
    return HI_SUCCESS;
}

int SamplePcivGetLocalId(int *local_id)
{
    int fd;
    struct hi_mcc_handle_attr attr;

    fd = open("/dev/mcc_userdev", O_RDWR);
    if (fd<=0)
    {
        printf("open mcc dev fail\n");
        return -1;
    }

    *local_id = ioctl(fd, HI_MCC_IOC_GET_LOCAL_ID, &attr);
    printf("pci local id is %d \n", *local_id);

    attr.target_id = 0;
    attr.port      = 0;
    attr.priority  = 0;
    ioctl(fd, HI_MCC_IOC_CONNECT, &attr);
    printf("===================close port %d!\n",attr.port);
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    HI_S32 s32Ret, s32EchoMsgLen = 0;
    VB_CONF_S stVbConf = {0};
    PCIV_BASEWINDOW_S stPciBaseWindow;
    SAMPLE_PCIV_MSG_S stMsg;

    /* wait for pci host ... */
    s32Ret = PCIV_WaitConnect(0);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    SamplePcivGetLocalId(&g_s32PciLocalId);

    /* open pci msg port for commom cmd */
    s32Ret = PCIV_OpenMsgPort(0, PCIV_MSGPORT_COMM_CMD);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    /* init mpp sys and vb */
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

    s32Ret = SAMPLE_InitMPP(&stVbConf);
    PCIV_CHECK_ERR(s32Ret);

    SAMPLE_COMM_SYS_MemConfig();

    /* get PF Window info of this pci device */
    stPciBaseWindow.s32ChipId = 0;
    s32Ret = HI_MPI_PCIV_GetBaseWindow(0, &stPciBaseWindow);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    g_u32PfAhbBase = stPciBaseWindow.u32PfAHBAddr;
    printf("PF AHB addr:0x%x\n", g_u32PfAhbBase);

    while (1)
    {
        s32EchoMsgLen = 0;
        s32Ret = PCIV_ReadMsg(0, PCIV_MSGPORT_COMM_CMD, &stMsg);
        if (s32Ret != HI_SUCCESS)
        {
            usleep(0);
            continue;
        }
        printf("\nreceive msg, MsgType:(%d,%s) \n",
            stMsg.stMsgHead.u32MsgType, PCIV_MSG_PRINT_TYPE(stMsg.stMsgHead.u32MsgType));

        switch(stMsg.stMsgHead.u32MsgType)
        {
            case SAMPLE_PCIV_MSG_INIT_MSG_PORG:
            {
                s32Ret = SamplePciv_SlaveInitPort(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_MSG_PORG:
            {
                s32Ret = SamplePciv_SlaveExitPort(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_VI:
            {
                s32Ret = SamplePciv_SlaveStartVi(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_VI:
            {
                s32Ret = SamplePciv_SlaveStopVi(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_START_VDEC:
            {
                s32Ret = SamplePciv_SlaveStartVdec(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_STOP_VDEC:
            {
                s32Ret = SamplePciv_SlaveStopVdec(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_START_VPSS:
            {
                s32Ret = SamplePciv_SlaveStartVpss(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_STOP_VPSS:
            {
                s32Ret = SamplePciv_SlaveStopVpss(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_CREATE_PCIV:
            {
                s32Ret = SamplePciv_SlaveStartPciv(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_DESTROY_PCIV:
            {
                s32Ret = SamplePciv_SlaveStopPicv(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_ALL_VENC:
            {
                s32Ret = SamplePciv_SlaveStartVenc(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_ALL_VENC:
            {
                s32Ret = SamplePciv_SlaveStopVenc(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_HOST_START_VENC:
            {
                s32Ret = SamplePciv_SlaveRecvVenc(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_STREAM_VDEC:
            {
                s32Ret = SamplePciv_SlaveStartVdecStream(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC:
            {
                s32Ret = SamplePciv_SlaveStopVdecStream(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_WIN_VB:
            {
                s32Ret = SamplePciv_SlaveInitWinVb(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_MALLOC:
            {
                s32Ret = SamplePciv_SlaveMalloc(&stMsg);
                s32EchoMsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
                break;
            }
            default:
            {
                printf("invalid msg, type:%d \n", stMsg.stMsgHead.u32MsgType);
                s32Ret = HI_FAILURE;
                break;
            }
        }
        /* echo msg to host */
        SamplePcivEchoMsg(s32Ret, s32EchoMsgLen, &stMsg);
    }

    /* exit */
    SAMPLE_ExitMPP();

    return HI_SUCCESS;
}


