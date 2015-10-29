#include "hi_type.h"
#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_comm_vi.h"
#include "hi_comm_vda.h"

#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vi.h"
#include "mpi_vda.h"

#include "mpi_wrap.h"
#include <stdio.h>

#define VDA_OD_CHN 1

typedef struct hiVDA_OD_PARAM_S
{
    HI_BOOL bThreadStart;
    VDA_CHN VdaChn;
}VDA_OD_PARAM_S;

static int now()
{
	struct timeval current;
	gettimeofday(&current, NULL);
	return current.tv_sec*1000*1000 + current.tv_usec;
}

int comm_sys_init()
{
	// XXXX:写死还是通过配置文件？
	VB_CONF_S vb_conf;
	vb_conf.u32MaxPoolCnt = 256;
	vb_conf.astCommPool[0].u32BlkSize = 1920 * 1080;
	vb_conf.astCommPool[0].u32BlkCnt = 8;
	vb_conf.astCommPool[0].acMmzName[0] = '\0';

	vb_conf.astCommPool[1].u32BlkSize = 1920 * 1080;
	vb_conf.astCommPool[1].u32BlkSize = 8;
	strcpy(vb_conf.astCommPool[1].acMmzName, "ddr1");

    MPP_SYS_CONF_S sys_conf = {0};
    int ret = -1;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    if (NULL  != pvb_conf)
    {
        PRT_ERR("input parameter is null, it is invaild!\n");
        return -1;
    }

    ret = HI_MPI_VB_SetConf(vb_conf);
    if (0 != ret)
    {
        PRT_ERR("HI_MPI_VB_SetConf failed!\n");
        return -1;
    }

    ret = HI_MPI_VB_Init();
    if (0 != ret)
    {
        PRT_ERR("HI_MPI_VB_Init failed!\n");
        return -1;
    }

    sys_conf.u32AlignWidth = SYS_ALIGN_WIDTH;
    ret = HI_MPI_SYS_SetConf(&sys_conf);
    if (0 != ret)
    {
        PRT_ERR("HI_MPI_SYS_SetConf failed\n");
        return -1;
    }

    ret = HI_MPI_SYS_Init();
    if (0 != ret)
    {
        PRT_ERR("HI_MPI_SYS_Init failed!\n");
        return -1;
    }

    return 0;
}

int comm_vi_memconfig(VI_MODE_E vi_mode)
{
    char * pmmz_name;
    MPP_CHN_S chn_vi;
    VI_PARAM_S vi_param;
    int vi_dev;
    int vi_chn;

    inti, j, ret;

    ret = comm_vi_mode2param(vi_mode, &vi_param);
    if (0 !=ret)
    {
        PRT_ERR("vi get param failed!\n");
        return -1;
    }

    j = 0;
    for(i=0; i<vi_param.s32ViChnCnt; i++)
    {
        vi_chn = i * vi_param.s32ViChnInterval;
        vi_dev = comm_vi_getdev(vi_mode, vi_chn);

        printf("dev:%d, chn:%d\n", vi_dev, vi_chn);
        if (vi_dev < 0)
        {
            PRT_ERR("get vi dev failed !\n");
            return -1;
        }

        pmmz_name = (0==i%2)?NULL: "ddr1";
        chn_vi.enModId = HI_ID_VIU;
        chn_vi.s32DevId = 0; //For VIU mode, this item must be set to zero
        chn_vi.s32ChnId = vi_chn;
        ret = HI_MPI_SYS_SetMemConf(&chn_vi,pmmz_name);
        if (ret)
        {
            PRT_ERR("VI HI_MPI_SYS_SetMemConf failed with %#x!\n", ret);
            return -1;
        }
		fprintf(stderr, " ch %d: using %s\n", vi_chn, pmmz_name);
        
        /* HD mode, we will start vi sub-chn */
        if (HI_TRUE == comm_vi_ishd(vi_mode))
        {
            vi_chn += 16;
            
            pmmz_name = (0==j%2)?NULL: "ddr1";
            chn_vi.enModId = HI_ID_VIU;
            chn_vi.s32DevId = 0; //For VIU mode, this item must be set to zero
            chn_vi.s32ChnId = vi_chn;
            ret = HI_MPI_SYS_SetMemConf(&chn_vi, pmmz_name);
            if (ret)
            {
                PRT_ERR("VI subchn HI_MPI_SYS_SetMemConf failed with %#x!\n", ret);
                return -1;
            }
			fprintf(stderr, " subch %d: using %s\n", vi_chn, pmmz_name);
            j++;
        }
    }

    return 0;
}

int comm_vi_start(VI_MODE_E vi_mode)
{
    VI_DEV ViDev;
    VI_CHN ViChn, vichn_sub;
    HI_S32 i;
    HI_S32 ret;
    VI_PARAM_S stViParam;
    SIZE_S stMainTargetSize;
    SIZE_S stSubTargetSize;
    RECT_S stCapRect;
    
    /*** get parameter from Sample_Vi_Mode ***/
    ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=ret)
    {
        PRT_ERR("vi get param failed!\n");
        return HI_FAILURE;
    }
    ret = comm_vi_mode2size(vi_mode, &stCapRect, &stMainTargetSize);
    if (HI_SUCCESS !=ret)
    {
        PRT_ERR("vi get size failed!\n");
        return HI_FAILURE;
    }
    
    /*** Start VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        ret = comm_vi_startdev(ViDev, vi_mode);
        if (HI_SUCCESS != ret)
        {
            PRT_ERR("comm_vi_startdev failed with %#x\n", ret);
            return HI_FAILURE;
        }
    }
    
    /*** Start VI Chn ***/
    for(i=0; i<stViParam.s32ViChnCnt; i++)
    {
        ViChn = i * stViParam.s32ViChnInterval;
        
        ret = comm_vi_startchn(ViChn, &stCapRect, &stMainTargetSize, vi_mode, VI_CHN_SET_NORMAL);
        if (HI_SUCCESS != ret)
        {
            PRT_ERR("call comm_vi_starchn failed with %#x\n", ret);
            return HI_FAILURE;
        } 
        /* HD mode, we will start vi sub-chn */
        if (HI_TRUE == comm_vi_ishd(vi_mode))
        {
			PRT_ERR("--- to start subch %d\n", SUBCHN(ViChn));
            vichn_sub = SUBCHN(ViChn);
            ret = comm_vi_getsubchnsize(vichn_sub, &stSubTargetSize);
            if (HI_SUCCESS != ret)
            {
                PRT_ERR("comm_vi_getsubchnsize(%d) failed!\n", vichn_sub);
                return HI_FAILURE;
            }
			PRT_ERR("\tsize=%d-%d\n", stSubTargetSize.u32Width, stSubTargetSize.u32Height);
            ret = comm_vi_startchn(vichn_sub, &stCapRect, &stSubTargetSize,vi_mode, VI_CHN_SET_NORMAL);
            if (HI_SUCCESS != ret)
            {
                PRT_ERR("comm_vi_startchn (Sub_Chn-%d) failed!\n", vichn_sub);
                return HI_FAILURE;
            }
        }
    }

    return HI_SUCCESS;
}

static void  comm_vi_readframe(FILE * fp, HI_U8 * pY, HI_U8 * pU, HI_U8 * pV, HI_U32 width, HI_U32 height, HI_U32 stride, HI_U32 stride2)
{
    HI_U8 * pDst;

    HI_U32 u32Row;

    pDst = pY;
    for ( u32Row = 0; u32Row < height; u32Row++ )
    {
        fread( pDst, width, 1, fp );
        pDst += stride;
    }

    pDst = pU;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }

    pDst = pV;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }
}

static int  comm_vi_plantosemi(HI_U8 *pY, HI_S32 yStride,
                       HI_U8 *pU, HI_S32 uStride,
                       HI_U8 *pV, HI_S32 vStride,
                       HI_S32 picWidth, HI_S32 picHeight)
{
    HI_S32 i;
    HI_U8* pTmpU, *ptu;
    HI_U8* pTmpV, *ptv;
    HI_S32 s32HafW = uStride >>1 ;
    HI_S32 s32HafH = picHeight >>1 ;
    HI_S32 s32Size = s32HafW*s32HafH;

    pTmpU = malloc( s32Size ); ptu = pTmpU;
    pTmpV = malloc( s32Size ); ptv = pTmpV;

    memcpy(pTmpU,pU,s32Size);
    memcpy(pTmpV,pV,s32Size);

    for(i = 0;i<s32Size>>1;i++)
    {
        *pU++ = *pTmpV++;
        *pU++ = *pTmpU++;

    }
    for(i = 0;i<s32Size>>1;i++)
    {
        *pV++ = *pTmpV++;
        *pV++ = *pTmpU++;
    }

    free( ptu );
    free( ptv );

    return HI_SUCCESS;
}

static int  comm_vi_getvframefromyuv(FILE *pYUVFile, HI_U32 u32Width, HI_U32 u32Height,HI_U32 u32Stride, VIDEO_FRAME_INFO_S *pstVFrameInfo)
{
    HI_U32             u32LStride;
    HI_U32             u32CStride;
    HI_U32             u32LumaSize;
    HI_U32             u32ChrmSize;
    HI_U32             u32Size;
    VB_BLK VbBlk;
    HI_U32 u32PhyAddr;
    HI_U8 *pVirAddr;

    u32LStride  = u32Stride;
    u32CStride  = u32Stride;

    u32LumaSize = (u32LStride * u32Height);
    u32ChrmSize = (u32CStride * u32Height) >> 2;/* YUV 420 */
    u32Size = u32LumaSize + (u32ChrmSize << 1);

    /* alloc video buffer block ---------------------------------------------------------- */
    VbBlk = HI_MPI_VB_GetBlock(VB_INVALID_POOLID, u32Size, NULL);
    if (VB_INVALID_HANDLE == VbBlk)
    {
        PRT_ERR("HI_MPI_VB_GetBlock err! size:%d\n",u32Size);
        return -1;
    }
    u32PhyAddr = HI_MPI_VB_Handle2PhysAddr(VbBlk);
    if (0 == u32PhyAddr)
    {
        return -1;
    }

    pVirAddr = (HI_U8 *) HI_MPI_SYS_Mmap(u32PhyAddr, u32Size);
    if (NULL == pVirAddr)
    {
        return -1;
    }

    pstVFrameInfo->u32PoolId = HI_MPI_VB_Handle2PoolId(VbBlk);
	printf("#### poolId = %d\n", pstVFrameInfo->u32PoolId);
    if (VB_INVALID_POOLID == pstVFrameInfo->u32PoolId)
    {
        return -1;
    }
    PRT_ERR("pool id :%d, phyAddr:%x,virAddr:%x\n" ,pstVFrameInfo->u32PoolId,u32PhyAddr,(int)pVirAddr);

    pstVFrameInfo->stVFrame.u32PhyAddr[0] = u32PhyAddr;
    pstVFrameInfo->stVFrame.u32PhyAddr[1] = pstVFrameInfo->stVFrame.u32PhyAddr[0] + u32LumaSize;
    pstVFrameInfo->stVFrame.u32PhyAddr[2] = pstVFrameInfo->stVFrame.u32PhyAddr[1] + u32ChrmSize;

    pstVFrameInfo->stVFrame.pVirAddr[0] = pVirAddr;
    pstVFrameInfo->stVFrame.pVirAddr[1] = pstVFrameInfo->stVFrame.pVirAddr[0] + u32LumaSize;
    pstVFrameInfo->stVFrame.pVirAddr[2] = pstVFrameInfo->stVFrame.pVirAddr[1] + u32ChrmSize;

    pstVFrameInfo->stVFrame.u32Width  = u32Width;
    pstVFrameInfo->stVFrame.u32Height = u32Height;
    pstVFrameInfo->stVFrame.u32Stride[0] = u32LStride;
    pstVFrameInfo->stVFrame.u32Stride[1] = u32CStride;
    pstVFrameInfo->stVFrame.u32Stride[2] = u32CStride;
    pstVFrameInfo->stVFrame.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pstVFrameInfo->stVFrame.u32Field = VIDEO_FIELD_FRAME;/* Intelaced D1,otherwise VIDEO_FIELD_FRAME */

    /* read Y U V data from file to the addr ----------------------------------------------*/
       comm_vi_readframe(pYUVFile, pstVFrameInfo->stVFrame.pVirAddr[0],
       pstVFrameInfo->stVFrame.pVirAddr[1], pstVFrameInfo->stVFrame.pVirAddr[2],
       pstVFrameInfo->stVFrame.u32Width, pstVFrameInfo->stVFrame.u32Height,
       pstVFrameInfo->stVFrame.u32Stride[0], pstVFrameInfo->stVFrame.u32Stride[1] >> 1 );

    /* convert planar YUV420 to sem-planar YUV420 -----------------------------------------*/
  	  comm_vi_plantosemi(pstVFrameInfo->stVFrame.pVirAddr[0], pstVFrameInfo->stVFrame.u32Stride[0],
      pstVFrameInfo->stVFrame.pVirAddr[1], pstVFrameInfo->stVFrame.u32Stride[1],
      pstVFrameInfo->stVFrame.pVirAddr[2], pstVFrameInfo->stVFrame.u32Stride[1],
      pstVFrameInfo->stVFrame.u32Width, pstVFrameInfo->stVFrame.u32Height);

    HI_MPI_SYS_Munmap(pVirAddr, u32Size);
	HI_MPI_VB_ReleaseBlock(VbBlk);
    
    return 0;
}

int comm_vda_odstart(int vda_chn, int  vi_chn, SIZE *pstSize, detect_t *det)
{
    VDA_CHN_ATTR_S stVdaChnAttr;
    MPP_CHN_S stSrcChn, stDestChn;
    HI_S32 ret = HI_SUCCESS;
    
    if (VDA_MAX_WIDTH < pstSize->u32Width || VDA_MAX_HEIGHT < pstSize->u32Height)
    {
        PRT_ERR("Picture size invaild!\n");
        return HI_FAILURE;
    }
    
    /********************************************
     step 1 : create vda channel
    ********************************************/
    stVdaChnAttr.enWorkMode = VDA_WORK_MODE_OD;
    stVdaChnAttr.u32Width   = pstSize->width;
    stVdaChnAttr.u32Height  = pstSize->height;

    stVdaChnAttr.unAttr.stOdAttr.enVdaAlg      = VDA_ALG_BG;
    stVdaChnAttr.unAttr.stOdAttr.enMbSize      = VDA_MB_16PIXEL;
    stVdaChnAttr.unAttr.stOdAttr.enMbSadBits   = VDA_MB_SAD_16BIT;
    stVdaChnAttr.unAttr.stOdAttr.enRefMode     = VDA_REF_MODE_DYNAMIC;
    stVdaChnAttr.unAttr.stOdAttr.u32VdaIntvl   = 4;
    stVdaChnAttr.unAttr.stOdAttr.u32BgUpSrcWgt = 128;
    
    stVdaChnAttr.unAttr.stOdAttr.u32RgnNum = 1;
    
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].stRect.s32X = 0;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].stRect.s32Y = 0;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].stRect.u32Width  = 320;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].stRect.u32Height = 64;

    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].u32SadTh      = 100;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].u32AreaTh     = 40;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].u32OccCntTh   = 1;
    stVdaChnAttr.unAttr.stOdAttr.astOdRgnAttr[0].u32UnOccCntTh = 0;
	
    ret = HI_MPI_VDA_CreateChn(vda_chn, &stVdaChnAttr);
    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return(ret);
    }

    /********************************************
     step 2 : bind vda channel to vi channel
    ********************************************/
		
	/* vda start rcv picture */
    ret = HI_MPI_VDA_StartRecvPic(vda_chn);

    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return(ret);
    }
	Send_Picture(vda_chn, "./save003.yuv");	
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = vi_chn;

    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = vda_chn;
	stDestChn.s32DevId = 0;

    ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return ret;
    }
		
    det->is_quit = false;
    det->vda_chn   = vda_chn;
	int ret = pthread_mutex_init(&det->mutex, NULL);
	if (0 != ret){
		perror("Mutex initialization failed");
		comm_sys_exit();
	}
	
    pthread_create(&(det->thread_id), 0, comm_vda_odgetresult, (HI_VOID *)det);

    return HI_SUCCESS;
}

void comm_vda_odstop(VDA_CHN VdaChn, VI_CHN ViChn)
{
    HI_S32 ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;

    /* join thread */
    if (HI_TRUE == gs_stOdParam.bThreadStart)
    {
        gs_stOdParam.bThreadStart = HI_FALSE;
        pthread_join(gs_VdaPid[VDA_OD_CHN], 0);
    }
	
    /* vda stop recv picture */
    ret = HI_MPI_VDA_StopRecvPic(VdaChn);
    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", ret);
    }

    /* unbind vda chn & vi chn */
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = ViChn;
    stSrcChn.s32DevId = 0;
    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = VdaChn;
    stDestChn.s32DevId = 0;
    ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", ret);
    }

    /* destroy vda chn */
    ret = HI_MPI_VDA_DestroyChn(VdaChn);
    if(ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n",ret);
    }
    return;
}

int comm_vi_stop(VI_MODE_E vi_mode)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    HI_S32 i;
    HI_S32 ret;
    VI_PARAM_S stViParam;

    /*** get parameter from Sample_Vi_Mode ***/
    ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=ret)
    {
        PRT_ERR("SAMPLE_COMM_VI_Mode2Param failed!\n");
        return HI_FAILURE;
    }

    /*** Stop VI Chn ***/
    for(i=0;i<stViParam.s32ViChnCnt;i++)
    {
        /* Stop vi phy-chn */
        ViChn = i * stViParam.s32ViChnInterval;
        ret = HI_MPI_VI_DisableChn(ViChn);
        if (HI_SUCCESS != ret)
        {
            PRT_ERR("SAMPLE_COMM_VI_StopChn failed with %#x\n",ret);
            return HI_FAILURE;
        }
        /* HD mode, we will stop vi sub-chn */
        if (HI_TRUE == comm_vi_ishd(vi_mode))
        {
            ViChn += 16;
            ret = HI_MPI_VI_DisableChn(ViChn);
            if (HI_SUCCESS != ret)
            {
                PRT_ERR("SAMPLE_COMM_VI_StopChn failed with %#x\n", ret);
                return HI_FAILURE;
            }
        }
    }

    /*** Stop VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        ret = HI_MPI_VI_DisableDev(ViDev);
        if (HI_SUCCESS != ret)
        {
            PRT_ERR("SAMPLE_COMM_VI_StopDev failed with %#x\n", ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

void *comm_vda_odgetresult(void *pdata)
{
    int i;
    int ret;
    unsigned int u32RgnNum;
	VDA_DATA_S stVdaData;
    VDA_OD_PARAM_S *pgs_stOdParam;

    detect_t *det = (detect_t*)pdata;
	
    int VdaChn    = det->vda_chn;
    while(HI_TRUE != det->is_quit)
    {
        ret = HI_MPI_VDA_GetData(VdaChn,&stVdaData,HI_TRUE);
        if(ret != HI_SUCCESS)
        {
            PRT_ERR("HI_MPI_VDA_GetData failed with %#x!\n", ret);
            return NULL;
        }

		u32RgnNum = stVdaData.unData.stOdData.u32RgnNum;

    	for(i=0; i<u32RgnNum; i++)
        {
			//FIXME: 注意 是否能同时检测 ...
            if(HI_TRUE == stVdaData.unData.stOdData.abRgnAlarm[i])
            { 
                printf("################VdaChn--%d, Rgn--%d,Occ!\n",VdaChn, i);
				ret = HI_MPI_VDA_ResetOdRegion(VdaChn,i);
                if(ret != HI_SUCCESS)
                {
		   			 PRT_ERR("HI_MPI_VDA_ResetOdRegion failed with %#x!\n", ret);
                    return NULL;
                }
            }
        }
		
		pthread_mutex_lock(&det->mutex);
		det->stamp = now();	
		for (i=0; i<u32RgnNum; i++)
			det->is_arms[i] = stVdaData.unData.stOdData.abRgnAlarm[i];		
		pthread_mutex_unlock(&det->mutex);
			
        ret = HI_MPI_VDA_ReleaseData(VdaChn,&stVdaData);
		if(ret != HI_SUCCESS)
        {
            PRT_ERR("HI_MPI_VDA_ReleaseData failed with %#x!\n", ret);
            return NULL;
        }

        usleep(200*1000);
    }
    return HI_NULL;
}

int comm_sys_getpicsize(PIC_SIZE_E enPicSize, SIZE *pstSize)
{
    switch (enPicSize)
    {
       case PIC_VGA:     /* 640 * 480 */
            pstSize->witdh = 640;
            pstSize->height = 480;
            break;
       case PIC_HD720:   /* 1280 * 720 */
            pstSize->width = 1280;
            pstSize->height = 720;
            break;
        case PIC_HD1080:  /* 1920 * 1080 */
            pstSize->width = 1920;
            pstSize->height = 1080;
            break;
        default:
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}

int comm_vi_mode2param(VI_MODE_E vi_mode, VI_PARAM_S *pvi_param)
{
    switch (vi_mode)
    {
        case VI_MODE_4_1080P:
            pvi_param->s32ViDevCnt = 4;
            pvi_param->s32ViDevInterval = 2;
            pvi_param->s32ViChnCnt = 4;
            pvi_param->s32ViChnInterval = 4;
            break;
        default:
            PRT_ERR("ViMode invaild!\n");
            return -1;
    }
    return 0;
}

int comm_vi_getdev(VI_MODE_E vi_mode, int vi_chn)
{
    HI_S32 ret, s32ChnPerDev;
    VI_PARAM_S stViParam;

    ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=ret)
    {
        PRT_ERR("vi get param failed!\n");
        return (VI_DEV)-1;
    }

    s32ChnPerDev = stViParam.s32ViChnCnt / stViParam.s32ViDevCnt;
    return (VI_DEV)(vi_chn /stViParam.s32ViChnInterval / s32ChnPerDev * stViParam.s32ViDevInterval);
}

HI_BOOL comm_vi_ishd(VI_MODE_E vi_mode)
{
    if (VI_MODE_4_720P == vi_mode || VI_MODE_4_1080P == vi_mode)
        return HI_TRUE;
    else
        return HI_FALSE;
}

int comm_vi_mode2size(VI_MODE_E vi_mode, RECT_S *pcap_rect, SIZE_S *pdest_size)
{
    pcap_rect->s32X = 0;
    pcap_rect->s32Y = 0;
    switch (vi_mode)
    {
        case VI_MODE_4_720P:
            pdest_size->u32Width = 1280;
            pdest_size->u32Height = 720;
            pcap_rect->u32Width = 1280;
            pcap_rect->u32Height = 720;
            break;
        case VI_MODE_4_1080P:
            pdest_size->u32Width = 1920;
            pdest_size->u32Height = 1080;
            pcap_rect->u32Width = 1920;
            pcap_rect->u32Height = 1080;
            break;
		default:
            PRT_ERR("vi mode invaild!\n");
            return HI_FAILURE;
    }

	return HI_SUCCESS;
}

int comm_vi_startdev(int vi_dev, VI_MODE_E vi_mode) {
    HI_S32 ret;
    VI_DEV_ATTR_S    stViDevAttr;
    memset(&stViDevAttr,0,sizeof(stViDevAttr));

    switch (vi_mode)
    {
        case VI_MODE_4_720P:
            memcpy(&stViDevAttr,&DEV_ATTR_7441_BT1120_720P,sizeof(stViDevAttr));
            comm_vi_setmask(ViDev,&stViDevAttr);
            break;
        case VI_MODE_4_1080P:
            memcpy(&stViDevAttr,&DEV_ATTR_7441_BT1120_1080P,sizeof(stViDevAttr));
            comm_vi_setmask(ViDev,&stViDevAttr);
            break;
        default:
            PRT_ERR("vi input type[%d] is invalid!\n", vi_mode);
            return HI_FAILURE;
    }

    
    ret = HI_MPI_VI_SetDevAttr(vi_dev, &stViDevAttr);
    if (ret != HI_SUCCESS)
    {
        PRT_ERR("HI_MPI_VI_SetDevAttr failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    ret = HI_MPI_VI_EnableDev(vi_dev);
    if (ret != HI_SUCCESS)
    {
        PRT_ERR("HI_MPI_VI_EnableDev failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

void comm_vi_setmask(int vi_dev, VI_DEV_ATTR_S *pdev_attr) {
    switch (vi_dev % 4)
    {
        case 0:
            pdev_attr->au32CompMask[0] = 0xFF000000;
            if (VI_MODE_BT1120_STANDARD == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0x00FF0000;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0x0;
            }
            break;
        case 1:
            pdev_attr->au32CompMask[0] = 0xFF0000;
            if (VI_MODE_BT1120_INTERLEAVED == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0x0;
            }
            break;
        case 2:
            pdev_attr->au32CompMask[0] = 0xFF00;
            if (VI_MODE_BT1120_STANDARD == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0xFF;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0x0;
            }

            #if HICHIP == HI3531_V100
                #ifndef HI_FPGA
                    if ((VI_MODE_BT1120_STANDARD != pdev_attr->enIntfMode)
                        && (VI_MODE_BT1120_INTERLEAVED != pdev_attr->enIntfMode))
                    {
                        pdev_attr->au32CompMask[0] = 0xFF0000; 
                    }
                #endif
            #endif
            
            break;
        case 3:
            pdev_attr->au32CompMask[0] = 0xFF;
            if (VI_MODE_BT1120_INTERLEAVED == pdev_attr->enIntfMode)
            {
                pdev_attr->au32CompMask[1] = 0x0;
            }
            break;
        default:
            HI_ASSERT(0);
    }

}

int comm_vi_startchn(int vi_chn, RECT_S *pstCapRect, SIZE_S *pstTarSize, 
    VI_MODE_E vi_mode, VI_CHN_SET_E enViChnSet)
{
    HI_S32 ret;
    VI_CHN_ATTR_S stChnAttr, attr;

    /* step  5: config & start vicap dev */
    memcpy(&stChnAttr.stCapRect, pstCapRect, sizeof(RECT_S));
    if (vi_mode_16_cif == vi_mode)
    {
        stChnAttr.enCapSel = VI_CAPSEL_BOTTOM;
    }
    else
    {
        stChnAttr.enCapSel = VI_CAPSEL_BOTH;
    }
    /* to show scale. this is a sample only, we want to show dist_size = D1 only */
    stChnAttr.stDestSize.u32Width = pstTarSize->u32Width;
    stChnAttr.stDestSize.u32Height = pstTarSize->u32Height;
    stChnAttr.enPixFormat = PIXEL_FORMAT;   /* sp420 or sp422 */
    stChnAttr.bMirror = (VI_CHN_SET_MIRROR == enViChnSet)?HI_TRUE:HI_FALSE;
    stChnAttr.bFlip = (VI_CHN_SET_FILP == enViChnSet)?HI_TRUE:HI_FALSE;

    stChnAttr.bChromaResample = HI_FALSE;
    stChnAttr.s32SrcFrameRate = -1;
    stChnAttr.s32FrameRate = -1;

    ret = HI_MPI_VI_SetChnAttr(vi_chn, &stChnAttr);
    if (ret != HI_SUCCESS)
    {
        PRT_ERR("failed with %#x!\n", ret);
        return HI_FAILURE;
    }

	ret = HI_MPI_VI_GetChnAttr(vi_chn, &attr);
	if (ret != HI_SUCCESS) {
		PRT_ERR("failed with %x\n", ret);
		return HI_FAILURE;
	}
	
	fprintf(stderr, "=== ch%d ===\n", vi_chn);
	fprintf(stderr, "\twidth,height: %d,%d\n", attr.stDestSize.u32Width, 
			attr.stDestSize.u32Height);
	fprintf(stderr, "\tcap size: %d, %d\n", attr.stCapRect.u32Width, 
			attr.stCapRect.u32Height);


    
    ret = HI_MPI_VI_EnableChn(vi_chn);
    if (ret != HI_SUCCESS)
    {
        PRT_ERR("failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

void comm_sys_exit(void)
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    exit(-1);
}

int comm_vi_getsubchnsize(int vichn_sub, SIZE_S *pstSize)
{
    VI_CHN ViChn;
    
    ViChn = vichn_sub - 16;

    if (0==(ViChn%4)) //(0,4,8,12) subchn max size is 960x1600
    {
        pstSize->u32Width = 720;
        pstSize->u32Height = 576;
//		pstSize->u32Width = 480;
//		pstSize->u32Height = 270;
    }
    else if (0==(ViChn%2)) //(2,6,10,14) subchn max size is 640x720
    {
        pstSize->u32Width = VI_SUBCHN_2_W;
        pstSize->u32Height = VI_SUBCHN_2_H;
    }
    else
    {
        PRT_ERR("Vi odd sub_chn(%d) is invaild!\n", vichn_sub);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

void  comm_vda_odstop(int  vda_chn, int  vi_chn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;

    /* join thread */
    if (HI_TRUE == gs_stOdParam.bThreadStart)
    {
        gs_stOdParam.bThreadStart = HI_FALSE;
        pthread_join(gs_VdaPid[SAMPLE_VDA_OD_CHN], 0);
    }
	
    /* vda stop recv picture */
    s32Ret = HI_MPI_VDA_StopRecvPic(vda_chn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", s32Ret);
    }

    /* unbind vda chn & vi chn */
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = vi_chn;
    stSrcChn.s32DevId = 0;
    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = vda_chn;
    stDestChn.s32DevId = 0;
    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", s32Ret);
    }

    /* destroy vda chn */
    s32Ret = HI_MPI_VDA_DestroyChn(vda_chn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n",s32Ret);
    }
    return;
}

int comm_vi_stop(VI_MODE_E vi_mode)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    HI_S32 i;
    HI_S32 s32Ret;
    SAMPLE_VI_PARAM_S stViParam;

    /*** get parameter from Sample_Vi_Mode ***/
    s32Ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        PRT_ERR("SAMPLE_COMM_VI_Mode2Param failed!\n");
        return HI_FAILURE;
    }

    /*** Stop VI Chn ***/
    for(i=0;i<stViParam.s32ViChnCnt;i++)
    {
        /* Stop vi phy-chn */
        ViChn = i * stViParam.s32ViChnInterval;
        s32Ret = HI_MPI_VI_DisableChn(ViChn);
        if (HI_SUCCESS != s32Ret)
        {
            PRT_ERR("SAMPLE_COMM_VI_StopChn failed with %#x\n",s32Ret);
            return HI_FAILURE;
        }
        /* HD mode, we will stop vi sub-chn */
        if (HI_TRUE == SAMPLE_COMM_VI_IsHd(vi_mode))
        {
            ViChn += 16;
            s32Ret = HI_MPI_VI_DisableChn(ViChn);
            if (HI_SUCCESS != s32Ret)
            {
                PRT_ERR("SAMPLE_COMM_VI_StopChn failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    /*** Stop VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        s32Ret = HI_MPI_VI_DisableDev(ViDev);
        if (HI_SUCCESS != s32Ret)
        {
            PRT_ERR("SAMPLE_COMM_VI_StopDev failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}
