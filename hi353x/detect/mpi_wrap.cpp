#include "mpi_wrap.h"
#include <stdio.h>

#define VDA_OD_CHN 1

typedef struct hiVDA_OD_PARAM_S
{
    HI_BOOL bThreadStart;
    VDA_CHN VdaChn;
}VDA_OD_PARAM_S;

static pthread_t gs_VdaPid[2];
static VDA_OD_PARAM_S gs_stOdParam;


int comm_sys_init(VB_CONF_S *pvb_conf)
{
    MPP_SYS_CONF_S sys_conf = {0};
    int ret = -1;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    if (NULL  != pvb_conf)
    {
        PRT_ERR("input parameter is null, it is invaild!\n");
        return -1;
    }

    ret = HI_MPI_VB_SetConf(pvb_conf);
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

int comm_vi_start(VI_MODE_E vi_mode, VIDEO_NORM_E norm)
{
    VI_DEV ViDev;
    VI_CHN ViChn, vichn_sub;
    HI_S32 i;
    HI_S32 s32Ret;
    VI_PARAM_S stViParam;
    SIZE_S stMainTargetSize;
    SIZE_S stSubTargetSize;
    RECT_S stCapRect;
    
    /*** get parameter from Sample_Vi_Mode ***/
    s32Ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        PRT_ERR("vi get param failed!\n");
        return HI_FAILURE;
    }
    s32Ret = comm_vi_mode2size(vi_mode, norm, &stCapRect, &stMainTargetSize);
    if (HI_SUCCESS !=s32Ret)
    {
        PRT_ERR("vi get size failed!\n");
        return HI_FAILURE;
    }
    
    /*** Start AD ***/
    s32Ret = comm_vi_adstart(vi_mode, norm);
    if (HI_SUCCESS !=s32Ret)
    {
        PRT_ERR("Start AD failed!\n");
        return HI_FAILURE;
    }
    
    /*** Start VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        s32Ret = comm_vi_startdev(ViDev, vi_mode);
        if (HI_SUCCESS != s32Ret)
        {
            PRT_ERR("comm_vi_startdev failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }
    }
    
    /*** Start VI Chn ***/
    for(i=0; i<stViParam.s32ViChnCnt; i++)
    {
        ViChn = i * stViParam.s32ViChnInterval;
        
        s32Ret = comm_vi_startchn(ViChn, &stCapRect, &stMainTargetSize, vi_mode, VI_CHN_SET_NORMAL);
        if (HI_SUCCESS != s32Ret)
        {
            PRT_ERR("call comm_vi_starchn failed with %#x\n", s32Ret);
            return HI_FAILURE;
        } 
        /* HD mode, we will start vi sub-chn */
        if (HI_TRUE == comm_vi_ishd(vi_mode))
        {
			PRT_ERR("--- to start subch %d\n", SUBCHN(ViChn));
            vichn_sub = SUBCHN(ViChn);
            s32Ret = comm_vi_getsubchnsize(vichn_sub, norm, &stSubTargetSize);
            if (HI_SUCCESS != s32Ret)
            {
                PRT_ERR("comm_vi_getsubchnsize(%d) failed!\n", vichn_sub);
                return HI_FAILURE;
            }
			PRT_ERR("\tsize=%d-%d\n", stSubTargetSize.u32Width, stSubTargetSize.u32Height);
            s32Ret = comm_vi_startchn(vichn_sub, &stCapRect, &stSubTargetSize,vi_mode, VI_CHN_SET_NORMAL);
            if (HI_SUCCESS != s32Ret)
            {
                PRT_ERR("comm_vi_startchn (Sub_Chn-%d) failed!\n", vichn_sub);
                return HI_FAILURE;
            }
        }
    }

    return HI_SUCCESS;
}

int comm_vda_odstart(int vda_chn, int  vi_chn, SIZE_S *pstSize)
{
	char c = 'v';
    VDA_CHN_ATTR_S stVdaChnAttr;
    MPP_CHN_S stSrcChn, stDestChn;
    HI_S32 s32Ret = HI_SUCCESS;
    
    if (VDA_MAX_WIDTH < pstSize->u32Width || VDA_MAX_HEIGHT < pstSize->u32Height)
    {
        PRT_ERR("Picture size invaild!\n");
        return HI_FAILURE;
    }
    
    /********************************************
     step 1 : create vda channel
    ********************************************/
    stVdaChnAttr.enWorkMode = VDA_WORK_MODE_OD;
    stVdaChnAttr.u32Width   = pstSize->u32Width;
    stVdaChnAttr.u32Height  = pstSize->u32Height;

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
	
    s32Ret = HI_MPI_VDA_CreateChn(vda_chn, &stVdaChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return(s32Ret);
    }

    /********************************************
     step 2 : bind vda channel to vi channel
    ********************************************/
		
	/* vda start rcv picture */
    s32Ret = HI_MPI_VDA_StartRecvPic(vda_chn);

    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return(s32Ret);
    }
	Send_Picture(vda_chn, "./save003.yuv");	
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = vi_chn;

    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = vda_chn;
	stDestChn.s32DevId = 0;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err!\n");
        return s32Ret;
    }
		
	
    gs_stOdParam.bThreadStart = HI_TRUE;
    gs_stOdParam.VdaChn   = vda_chn;
    pthread_create(&gs_VdaPid[VDA_OD_CHN], 0, comm_vda_odgetresult, (HI_VOID *)&gs_stOdParam);

    return HI_SUCCESS;
}

void comm_vda_odstop(VDA_CHN VdaChn, VI_CHN ViChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;

    /* join thread */
    if (HI_TRUE == gs_stOdParam.bThreadStart)
    {
        gs_stOdParam.bThreadStart = HI_FALSE;
        pthread_join(gs_VdaPid[VDA_OD_CHN], 0);
    }
	
    /* vda stop recv picture */
    s32Ret = HI_MPI_VDA_StopRecvPic(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", s32Ret);
    }

    /* unbind vda chn & vi chn */
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = ViChn;
    stSrcChn.s32DevId = 0;
    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = VdaChn;
    stDestChn.s32DevId = 0;
    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        PRT_ERR("err(0x%x)!!!!\n", s32Ret);
    }

    /* destroy vda chn */
    s32Ret = HI_MPI_VDA_DestroyChn(VdaChn);
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
    VI_PARAM_S stViParam;

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
        if (HI_TRUE == comm_vi_ishd(vi_mode))
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

void *comm_vda_odgetresult(void *pdata)
{
	FILE *fp;
	struct timeval start;
    HI_S32 i;
    HI_S32 s32Ret;
    VDA_CHN VdaChn;
    VDA_DATA_S stVdaData;
    HI_U32 u32RgnNum;
    VDA_OD_PARAM_S *pgs_stOdParam;
    //FILE *fp=stdout;
	int num = 0;
    pgs_stOdParam = (VDA_OD_PARAM_S *)pdata;

    VdaChn    = pgs_stOdParam->VdaChn;
    
    while(HI_TRUE == pgs_stOdParam->bThreadStart)
    {
        s32Ret = HI_MPI_VDA_GetData(VdaChn,&stVdaData,HI_TRUE);
        if(s32Ret != HI_SUCCESS)
        {
            PRT_ERR("HI_MPI_VDA_GetData failed with %#x!\n", s32Ret);
            return NULL;
        }
	//	fprintf(stdout, "HI_MPI_VDA_GetData od succeed\n");
	    //COMM_VDA_OdPrt(fp, &stVdaData);

		u32RgnNum = stVdaData.unData.stOdData.u32RgnNum;

	//	fprintf(stdout, "u32RgnNum = %d\n", u32RgnNum);		
    	for(i=0; i<u32RgnNum; i++)
        {
            if(HI_TRUE == stVdaData.unData.stOdData.abRgnAlarm[i])
            { 
				num++;
                printf("################VdaChn--%d, no--%d, Rgn--%d,Occ!\n",VdaChn, num, i);
			//	gettimeofday(&start, NULL);
       //         Log("\n");
				s32Ret = HI_MPI_VDA_ResetOdRegion(VdaChn,i);
		//		Print_Time_Diff(start);
                if(s32Ret != HI_SUCCESS)
                {
		   			 PRT_ERR("HI_MPI_VDA_ResetOdRegion failed with %#x!\n", s32Ret);
                    return NULL;
                }
            }
        }
			
        s32Ret = HI_MPI_VDA_ReleaseData(VdaChn,&stVdaData);
		if(s32Ret != HI_SUCCESS)
        {
            PRT_ERR("HI_MPI_VDA_ReleaseData failed with %#x!\n", s32Ret);
            return NULL;
        }

        usleep(200*1000);
    }
    return HI_NULL;
}

int comm_sys_getpicsize(VIDEO_NORM_E norm, PIC_SIZE_E enPicSize, SIZE_S *pstSize)
{
    switch (enPicSize)
    {
       case PIC_VGA:     /* 640 * 480 */
            pstSize->u32Width = 640;
            pstSize->u32Height = 480;
            break;
       case PIC_HD720:   /* 1280 * 720 */
            pstSize->u32Width = 1280;
            pstSize->u32Height = 720;
            break;
        case PIC_HD1080:  /* 1920 * 1080 */
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1080;
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
    HI_S32 s32Ret, s32ChnPerDev;
    VI_PARAM_S stViParam;

    s32Ret = comm_vi_mode2param(vi_mode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
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

int comm_vi_mode2size(VI_MODE_E vi_mode, VIDEO_NORM_E norm, RECT_S *pcap_rect, SIZE_S *pdest_size)
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

int comm_vi_adstart(VI_MODE_E vi_mode, VIDEO_NORM_E norm)
{
    HI_S32 s32Ret;
    
    switch (vi_mode)
    {
        case VI_MODE_4_720P:
            break;
        case VI_MODE_4_1080P:
            break;
        default:
            PRT_ERR("AD not support!\n");
            return HI_FAILURE;
    }
    
    return HI_SUCCESS;
}

int comm_vi_startdev(int vi_dev, VI_MODE_E vi_mode) {
    HI_S32 s32Ret;
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

    
    s32Ret = HI_MPI_VI_SetDevAttr(vi_dev, &stViDevAttr);
    if (s32Ret != HI_SUCCESS)
    {
        PRT_ERR("HI_MPI_VI_SetDevAttr failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableDev(vi_dev);
    if (s32Ret != HI_SUCCESS)
    {
        PRT_ERR("HI_MPI_VI_EnableDev failed with %#x!\n", s32Ret);
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
    HI_S32 s32Ret;
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

    s32Ret = HI_MPI_VI_SetChnAttr(vi_chn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        PRT_ERR("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

	s32Ret = HI_MPI_VI_GetChnAttr(vi_chn, &attr);
	if (s32Ret != HI_SUCCESS) {
		PRT_ERR("failed with %x\n", s32Ret);
		return HI_FAILURE;
	}
	
	fprintf(stderr, "=== ch%d ===\n", vi_chn);
	fprintf(stderr, "\twidth,height: %d,%d\n", attr.stDestSize.u32Width, 
			attr.stDestSize.u32Height);
	fprintf(stderr, "\tcap size: %d, %d\n", attr.stCapRect.u32Width, 
			attr.stCapRect.u32Height);


    
    s32Ret = HI_MPI_VI_EnableChn(vi_chn);
    if (s32Ret != HI_SUCCESS)
    {
        PRT_ERR("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

void comm_sys_exit(void)
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    return;
}

int comm_vi_getsubchnsize(int vichn_sub, VIDEO_NORM_E norm, SIZE_S *pstSize)
{
    VI_CHN ViChn;
    
    ViChn = vichn_sub - 16;

    if (0==(ViChn%4)) //(0,4,8,12) subchn max size is 960x1600
    {
        pstSize->u32Width = 720;
        pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==norm)?576:480;
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
