/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

******************************************************************************
  File Name     : sample_common.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/07/04
  Description   : common functions for sample.
                  In default,
                  VI capture TV signals.
                  VO display on TV screen.
  History       :
  1.Date        : 2009/07/04
    Author      : Hi3520MPP
    Modification: Created file.
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: Add video loss detect demo
******************************************************************************/
#ifdef __cplusplus
 #if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include "sample_common.h"
#include "sample_comm.h"

#include "tw2865.h"
#include "tw2960.h"

#define TW2865_FILE   "/dev/tw2865dev"

/************!!!!attention:********
The test code havn't check whether vi dev is out of  range(0-3)
*********************************/
#define G_VIDEV_START 0

/* video input into VI.
** Only valid when input from ADC, such as TW2864, TW2815, etc.
** VI samples that run for PAL will change to NTSC if modify it
** to VIDEO_ENCODING_MODE_NTSC.
*/
#if 1
    VIDEO_NORM_E   gs_enViNorm   = VIDEO_ENCODING_MODE_PAL;
    VO_INTF_SYNC_E gs_enSDTvMode = VO_OUTPUT_PAL;
#else
    VIDEO_NORM_E   gs_enViNorm   = VIDEO_ENCODING_MODE_NTSC;
    VO_INTF_SYNC_E gs_enSDTvMode = VO_OUTPUT_NTSC;
#endif
#if 1
VI_DEV_ATTR_S DEV_ATTR_BT656D1_4MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_4Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_BT656D1_2MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_2Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_BT656D1_1MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_7441_BT1120_1080P =
/* 典型时序3:7441 BT1120 1080P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_STANDARD,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0xFF0000},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1920,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            1080,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};


VI_DEV_ATTR_S DEV_ATTR_7441_BT1120_720P =
/* 典型时序3:7441 BT1120 720P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_STANDARD,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF00,    0xFF},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1280,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            720,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};
#endif
#if 1
VI_DEV_ATTR_S DEV_ATTR_7441_INTERLEAVED_720P =
/* 典型时序3:7441 BT1120 720P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_INTERLEAVED,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1280,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            720,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};
#endif

/************************************************************************************/
const HI_U8 g_SOI[2] = {0xFF, 0xD8};
const HI_U8 g_EOI[2] = {0xFF, 0xD9};
#if 0
/*****************************************************************************
 Prototype       : SAMPLE_ADV7441_CfgV
 Description     : config ADV7441
 Input           : enFormat  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_ADV7441_CfgV(ADV7441_VIDEO_FORMAT_E enFormat)
{
    HI_S32 adv7441_fd;

    adv7441_fd = open(ADV7441_DEV_FILE, O_RDWR);
    if (adv7441_fd < 0)
    {
        printf("warning: open adv7441 dev failed\n");
        return -1;
    }
/*now enFormat support  0:VIDEO_FORMAT_720P_60HZ,   1: VIDEO_FORMAT_1080I_60HZ*/
if (ioctl(adv7441_fd, ADV7441_SET_VIDEO_FORMAT, &enFormat))
    {
        printf("set adv7441 video format failed\n");
        close(adv7441_fd);
        return -1;
    }

    close(adv7441_fd);
    return 0;
}

/*****************************************************************************
 Prototype       : SAMPLE_TW2864_CfgV
 Description     : config video of tw2864
 Input           : enVideoMode  **
                   enWorkMode   **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : in default, TW2864 works in 4D1 mode.
*****************************************************************************/
HI_S32 SAMPLE_TW2864_CfgV(VI_WORK_MODE_E enViWorkMode)
{
    HI_S32 i, j, s32Ret;
    HI_S32 fd2864a = -1;
    HI_S32 fd2864b = -1;
    HI_S32 fd2864[2];
    HI_S32 s32VideoMode;
    tw2864_set_videomode stNorm;

    fd2864a = open(TW2864_A_FILE, O_RDWR);
    fd2864b = open(TW2864_B_FILE, O_RDWR);
    if ((fd2864b < 0) || (fd2864a < 0))
    {
        printf("can't open tw2864,fd(%d,%d)\n", fd2864a, fd2864b);
        return HI_FAILURE;
    }

    fd2864[0] = fd2864a;
    fd2864[1] = fd2864b;

/*in driver tw2864,define as 0:auto;1:NTSC;2:PAL */
    s32VideoMode = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 2 : 1;

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 4; j++)
        {
            stNorm.ch   = j;
            stNorm.mode = s32VideoMode;
            s32Ret = ioctl(fd2864[i], TW2864_SET_VIDEO_MODE, &stNorm);
            if (0 != s32Ret)
            {
                printf("set 2864(%d) video mode fail;err:%x!\n", i, s32Ret);
                return s32Ret;
            }
        }
    }

    close(fd2864a);
    close(fd2864b);
    printf("============================================\n");
    return HI_SUCCESS;
}
#endif
/*****************************************************************************
 Prototype       : SAMPLE_TW2865_CfgV
 Description     : config video of tw2865
 Input           : enVideoMode  **
                   enWorkMode   **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : in default, TW2865 works in 4D1 mode.
*****************************************************************************/
HI_S32 SAMPLE_TW2865_CfgV(VIDEO_NORM_E enVideoMode,VI_WORK_MODE_E enWorkMode)
{
    int fd, i;
    int video_mode;
    tw2865_video_norm stVideoMode;
    tw2865_work_mode work_mode;
#ifdef hi3515
    int chip_cnt = 2;
#else
    int chip_cnt = 4;
#endif

    fd = open(TW2865_FILE, O_RDWR);
    if (fd < 0)
    {
        printf("open %s fail\n", TW2865_FILE);
        return -1;
    }

    video_mode = (VIDEO_ENCODING_MODE_PAL == enVideoMode) ? TW2865_PAL : TW2865_NTSC ;

    for (i=0; i<chip_cnt; i++)
    {
        stVideoMode.chip    = i;
        stVideoMode.mode    = video_mode;
        if (ioctl(fd, TW2865_SET_VIDEO_NORM, &stVideoMode))
        {
            printf("set tw2865(%d) video mode fail\n", i);
            close(fd);
            return -1;
        }
    }

    for (i=0; i<chip_cnt; i++)
    {
        work_mode.chip = i;
        if (VI_WORK_MODE_4Multiplex == enWorkMode)
        {
            work_mode.mode = TW2865_4D1_MODE;
        }
        else if (VI_WORK_MODE_2Multiplex == enWorkMode)
        {
            work_mode.mode = TW2865_2D1_MODE;
        }
        else
        {
            printf("work mode not support\n");
            return -1;
        }
        ioctl(fd, TW2865_SET_WORK_MODE, &work_mode);
    }

    close(fd);
    return 0;
}
#if 0
/*****************************************************************************
 Prototype       : SAMPLE_MT9D131_CfgV
 Description     : config video of MT9D131
 Input           : val  ** 1: VGA  4: 2.0M
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
*****************************************************************************/
HI_S32 SAMPLE_MT9D131_CfgV(HI_S32 val)
{
    HI_S32 fd, tmp;

    fd = open("/dev/mt9d131", O_RDWR);
    if (fd < 0)
    {
        printf("open dev mt9d131 err\n");
        return -1;
    }

/*in driver mt9d131,IMAGESIZE only support :01:DC_VAL_VGA,04:DC_VAL_UXGA*/
    tmp = val;
    if (ioctl(fd, DC_SET_IMAGESIZE, &tmp))
    {
        perror("set mt9d131 size err");
        return -1;
    }
/*in driver mt9d131,POWERFREQ only support :01:DC_VAL_50HZ,02:DC_VAL_60HZ*/
    tmp = DC_VAL_60HZ;
    if (ioctl(fd, DC_SET_POWERFREQ, &tmp))
    {
        perror("set mt9d131 power freq err");
        return -1;
    }

    printf("set mt9d131 %d ok\n", val);
    close(fd);
    return 0;
}

typedef struct
{
    int start;
    pthread_t Pid;
    int chn_cnt;
} SAMPLE_VIDEO_LOSS;

static SAMPLE_VIDEO_LOSS s_stVideoLoss;


/* If your ADC stop output signal when NoVideo, you can open this macro */
//#define VDET_USE_VI

#ifdef VDET_USE_VI
static HI_S32 s_astViLastIntCnt[VIU_MAX_CHN_NUM] = {0};
void *SampleVLossDetProc(void *parg)
{
    int i;
    VI_DEV ViDev;
    VI_CHN ViChn;
    VI_SRC_CFG_S stSrcCfg;
    VI_DBG_INFO_S stDbgInfo;
    SAMPLE_VIDEO_LOSS *ctl = (SAMPLE_VIDEO_LOSS*)parg;

    for (i = 0; i < ctl->chn_cnt; i++)
    {
#ifdef hi3515
            ViDev = (i/4)*2;
#else
            ViDev = i/4;
#endif
            ViChn = i%4;
            HI_MPI_VI_GetSrcCfg(ViDev, ViChn, &stSrcCfg);
            stSrcCfg.bEnNoIntDet = HI_TRUE;
            HI_MPI_VI_SetSrcCfg(ViDev, ViChn, &stSrcCfg);
    }

    while (ctl->start)
    {
        for (i = 0; i < ctl->chn_cnt; i++)
        {
#ifdef hi3515
            ViDev = (i/4)*2;
#else
            ViDev = i/4;
#endif
            ViChn = i%4;

            if (HI_MPI_VI_GetDbgInfo(ViDev, ViChn, &stDbgInfo))
            {
                continue;
            }
            if (stDbgInfo.u32IntCnt == s_astViLastIntCnt[i])
            {
                printf("VI(%d,%d) int lost ,cnt:%d \n", ViDev, ViChn, stDbgInfo.u32IntCnt);
                //HI_MPI_VI_EnableUserPic(ViDev, ViChn);  //add by Mike
            }
            else
            {
                //HI_MPI_VI_DisableUserPic(ViDev, ViChn);  //add by Mike
            }
            s_astViLastIntCnt[i] = stDbgInfo.u32IntCnt;
        }
        usleep(500*1000);
    }

    ctl->start = 0;
    return NULL;
}
#else
void *SampleVLossDetProc(void *parg)
{
    int fd, i;
    VI_DEV ViDev;
    VI_CHN ViChn;
    tw2865_video_loss video_loss;
    SAMPLE_VIDEO_LOSS *ctl = (SAMPLE_VIDEO_LOSS*)parg;

    fd = open(TW2865_FILE, O_RDWR);
    if (fd < 0)
    {
        printf("open %s fail\n", TW2865_FILE);
        ctl->start = 0;
        return NULL;
    }

    while (ctl->start)
    {
        for (i = 0; i < ctl->chn_cnt; i++)
        {
            video_loss.chip = i/4;
            video_loss.ch   = i%4;
            ioctl(fd, TW2865_GET_VIDEO_LOSS, &video_loss);

#ifdef hi3515
            ViDev = (i/4)*2;
#else
            ViDev = i/4;
#endif
            ViChn = i%4;
            if (video_loss.is_lost)
            {
                //HI_MPI_VI_EnableUserPic(ViDev, ViChn);   //add by Mike
            }
            else
            {
                //HI_MPI_VI_DisableUserPic(ViDev, ViChn);  //add by Mike
            }
        }
        usleep(500000);
    }

    close(fd);
    ctl->start = 0;
    return NULL;
}
#endif

void SAMPLE_StartVLossDet(int chn_cnt)
{
    s_stVideoLoss.start = 1;
    s_stVideoLoss.chn_cnt = chn_cnt;
    pthread_create(&s_stVideoLoss.Pid, 0, SampleVLossDetProc, &s_stVideoLoss);
}

void SAMPLE_StopVLossDet()
{
    if (s_stVideoLoss.start)
    {
        s_stVideoLoss.start = 0;
        pthread_join(s_stVideoLoss.Pid, 0);
    }
}
#endif
/*****************************************************************************
 Prototype       : SAMPLE_InitMpp
 Description     : Init Mpp
 Input           : pstVbConf  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2009/7/4
    Author       : c55300
    Modification : Created function

*****************************************************************************/
HI_S32 SAMPLE_InitMPP(VB_CONF_S *pstVbConf)
{
    MPP_SYS_CONF_S stSysConf = {0};

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    if (HI_MPI_VB_SetConf(pstVbConf))
    {
        printf("HI_MPI_VB_SetConf failed!\n");
        return -1;
    }

    if (HI_MPI_VB_Init())
    {
        printf("HI_MPI_VB_Init failed!\n");
        return -1;
    }

    stSysConf.u32AlignWidth = 16;
    if (HI_MPI_SYS_SetConf(&stSysConf))
    {
        printf("conf : system config failed!\n");
        return -1;
    }

    if (HI_MPI_SYS_Init())
    {
        printf("sys init failed!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************
 Prototype       : SAMPLE_ExitMpp
 Description     : Exit Mpp
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_ExitMPP(HI_VOID)
{
    if (HI_MPI_SYS_Exit())
    {
        printf("sys exit fail\n");
        return -1;
    }

    if (HI_MPI_VB_Exit())
    {
        printf("vb exit fail\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************
 Prototype       : SampleVichnPerDev
 Description     : get number of channel of VI device with difference mode.
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
static HI_S32 SampleVichnPerDev(VI_INPUT_MODE_E enViInputMode, VI_WORK_MODE_E enViWorkMode)
{
    if ((VI_MODE_BT656 == enViInputMode) && (VI_WORK_MODE_1Multiplex == enViWorkMode))
    {
        return 1;
    }
    else if ((VI_MODE_BT656 == enViInputMode) && (VI_WORK_MODE_2Multiplex == enViWorkMode))
    {
        return 2;
    }
    else if ((VI_MODE_BT656 == enViInputMode) && (VI_WORK_MODE_4Multiplex == enViWorkMode))
    {
        return 4;
    }
    //else if ((VI_MODE_BT656 == enViInputMode) && (VI_WORK_MODE_4HALFD1 == enViWorkMode)) //add by Mike
   // {
    //    return 4;
    //}
    //else if ((VI_MODE_BT601 == enViInputMode) || (VI_MODE_DIGITAL_CAMERA == enViInputMode)
    //         || (VI_MODE_BT1120_PROGRESSIVE == enViInputMode) || (VI_INPUT_MODE_INTERLEAVED == enViInputMode))
    //{
    //    return 1;
    //}
    else
    {
        return 0;
    }
}


#if 1
HI_VOID SAMPLE_COMM_VI_SetMask(VI_DEV ViDev, VI_DEV_ATTR_S *pstDevAttr)
{
    switch (ViDev % 4)
    {
        case 0:
            pstDevAttr->au32CompMask[0] = 0xFF000000;
            if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x00FF0000;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        case 1:
            pstDevAttr->au32CompMask[0] = 0xFF0000;
            if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        case 2:
            pstDevAttr->au32CompMask[0] = 0xFF00;
            if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0xFF;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }

            #if HICHIP == HI3531_V100
                #ifndef HI_FPGA
                    if ((VI_MODE_BT1120_STANDARD != pstDevAttr->enIntfMode)
                        && (VI_MODE_BT1120_INTERLEAVED != pstDevAttr->enIntfMode))
                    {
                        /* 3531的ASIC板是两个BT1120口出16D1，此时dev2/6要设成dev1/5的MASK */
                        pstDevAttr->au32CompMask[0] = 0xFF0000;
                    }
                #endif
            #endif

            break;
        case 3:
            pstDevAttr->au32CompMask[0] = 0xFF;
            if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        default:
            HI_ASSERT(0);
    }
}
#endif


HI_S32 SAMPLE_TW2960_CfgV(VIDEO_NORM_E enVideoMode,VI_WORK_MODE_E enWorkMode)
{
    int fd, i;
    int video_mode;
    tw2865_video_norm stVideoMode;
    tw2865_work_mode work_mode;

    int chip_cnt = 4;

    fd = open("/dev/tw2960dev", O_RDWR);
    if (fd < 0)
    {
        printf("open 2960 fail\n");
        return -1;
    }

    video_mode = (VIDEO_ENCODING_MODE_PAL == enVideoMode) ? TW2960_PAL : TW2960_NTSC ;

    for (i=0; i<chip_cnt; i++)
    {
        stVideoMode.chip    = i;
        stVideoMode.mode    = video_mode;
        if (ioctl(fd, TW2960_SET_VIDEO_NORM, &stVideoMode))
        {
            printf("set tw2865(%d) video mode fail\n", i);
            close(fd);
            return -1;
        }
    }

    for (i=0; i<chip_cnt; i++)
    {
        work_mode.chip = i;
        if (VI_WORK_MODE_4Multiplex== enWorkMode)
        {
            work_mode.mode = TW2960_4D1_MODE;
        }
        else if (VI_WORK_MODE_2Multiplex== enWorkMode)
        {
            work_mode.mode = TW2960_2D1_MODE;
        }
        else if (VI_WORK_MODE_1Multiplex == enWorkMode)
        {
            work_mode.mode = TW2960_1D1_MODE;
        }
        else
        {
            printf("work mode not support\n");
            return -1;
        }
        ioctl(fd, TW2960_SET_WORK_MODE, &work_mode);
    }

    close(fd);
    return 0;
}


/*****************************************************************************
 Prototype       : SAMPLE_GetViChnPerDev
 Description     : with different mode, vi device can enter different number of
                   channel. call this function to get.
 Input           : ViDev  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  Remark         : vi device attribute must be set first.
*****************************************************************************/
HI_S32 SAMPLE_GetViChnPerDev(VI_DEV ViDev)
{
    //VI_PUB_ATTR_S stPubAttr;  //add by Mike
    VI_DEV_ATTR_S stPubAttr; //add by Mike
    HI_S32 s32ViChnPerDev;
    HI_S32 s32Ret;

    s32Ret = HI_MPI_VI_GetDevAttr(ViDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("Get vi(%d) channel per dev fail\n", ViDev);
        return 0;
    }

    s32ViChnPerDev = SampleVichnPerDev(stPubAttr.enIntfMode, stPubAttr.enWorkMode);
    return s32ViChnPerDev;
}

HI_S32 SAMPLE_PicSize2WH(PIC_SIZE_E enPicSize, HI_S32* s32Width, HI_S32* s32Height)
{
    switch ( enPicSize )
    {
        case PIC_QCIF:
            *s32Width = 176;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 144 : 120;
            break;
        case PIC_CIF:
            *s32Width = 352;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
            break;
        case PIC_2CIF:
            *s32Width = 352;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 576 : 480;
            break;
        case PIC_HD1:
            *s32Width = 704;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
            break;
        case PIC_D1:
            *s32Width = 704;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 576 : 480;
            break;
        case PIC_QVGA:
            *s32Width = 320;
            *s32Height = 240;
            break;
        case PIC_VGA:
            *s32Width = 640;
            *s32Height = 480;
            break;
        case PIC_XGA:
            *s32Width = 1024;
            *s32Height = 768;
            break;
        case PIC_SXGA:
            *s32Width = 1400;
            *s32Height = 1050;
            break;
        case PIC_UXGA:
            *s32Width = 1600;
            *s32Height = 1200;
            break;
        case PIC_WSXGA:
            *s32Width = 1680;
            *s32Height = 1050;
            break;
        default:
            printf("err pic size\n");
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SampleParseVoOutput
 Description     : parse vo output, to decide dispaly widht,height and rate.
 Input           : enVoOutput     **
 Output          : s32Width       **
                   s32Height      **
                   s32Rate        **
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2009/7/27
    Author       : c55300
    Modification : Created function

*****************************************************************************/
HI_S32 SampleParseVoOutput(VO_INTF_SYNC_E enVoOutput,
    HI_S32* s32Width, HI_S32* s32Height, HI_S32* s32DisplayRate)
{
    switch ( enVoOutput )
    {
        case VO_OUTPUT_PAL:
            *s32Width = 720;
            *s32Height = 576;
            *s32DisplayRate = 25;
            break;
        case VO_OUTPUT_NTSC:
            *s32Width = 720;
            *s32Height = 480;
            *s32DisplayRate = 30;
            break;
        case VO_OUTPUT_720P60:
            *s32Width = 1280;
            *s32Height = 720;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1080I60:
            *s32Width = 1920;
            *s32Height = 1080;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1080P30:
            *s32Width = 1920;
            *s32Height = 1080;
            *s32DisplayRate = 30;
            break;
        case VO_OUTPUT_800x600_60:
            *s32Width = 800;
            *s32Height = 600;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1024x768_60:
            *s32Width = 1024;
            *s32Height = 768;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1280x1024_60:
            *s32Width = 1280;
            *s32Height = 1024;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1366x768_60:
            *s32Width = 1366;
            *s32Height = 768;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1440x900_60:
            *s32Width = 1440;
            *s32Height = 900;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_USER:
            printf("Why call me? You should set display size in VO_OUTPUT_USER.\n");
            break;
        default:
            *s32Width = 720;
            *s32Height = 576;
            *s32DisplayRate = 25;
            printf(" vo display size is (720, 576).\n");
            break;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_StartViDev(VI_INTF_MODE_E enInputMode)
{
    HI_S32 s32Ret;
    VI_DEV ViDev;
    //VI_PUB_ATTR_S stViDevAttr; //add by Mike
    VI_DEV_ATTR_S stViDevAttr;

    stViDevAttr.enIntfMode = enInputMode;
    if (VI_MODE_BT656 == stViDevAttr.enIntfMode)
    {
        stViDevAttr.enWorkMode = VI_WORK_MODE_4Multiplex;
    }
    else
    {
        printf("%s now not support this input mode\n", __FUNCTION__);
        return HI_FAILURE;
    }

    SAMPLE_TW2865_CfgV(gs_enViNorm, VI_WORK_MODE_4Multiplex);

    for (ViDev=0; ViDev < VIU_MAX_DEV_NUM; ViDev++)
    {
        s32Ret = HI_MPI_VI_SetDevAttr(ViDev, &stViDevAttr);
        if (s32Ret)
        {
            printf("set vi dev %d attr fail,%x\n", ViDev, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VI_EnableDev(ViDev);  //add by Mike
        if (s32Ret)
        {
            printf("VI dev %d enable fail \n", ViDev);
            return s32Ret;
        }
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_StartViChn(VI_DEV ViDev, VI_CHN ViChn, PIC_SIZE_E enPicSize)
{
    HI_S32 s32Ret;
    VI_CHN_ATTR_S stViChnAttr;

    stViChnAttr.stDestSize.u32Width  = 704;
    stViChnAttr.stDestSize.u32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
    stViChnAttr.stCapRect.s32X       = 8;
    stViChnAttr.stCapRect.s32Y       = 0;
    stViChnAttr.stCapRect.u32Width   = stViChnAttr.stDestSize.u32Width;
    stViChnAttr.stCapRect.u32Height  = stViChnAttr.stDestSize.u32Height;
    stViChnAttr.enPixFormat          = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stViChnAttr.bChromaResample      = HI_FALSE;

    /* different pic size has different capture method for BT656. */
    stViChnAttr.enCapSel = (PIC_D1 == enPicSize || PIC_2CIF == enPicSize) ? \
                             VI_CAPSEL_BOTH : VI_CAPSEL_BOTTOM;

    s32Ret = HI_MPI_VI_SetChnAttr(ViChn, &stViChnAttr); //add by Mike
    if (s32Ret)
    {
        printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_EnableChn(ViChn);  //add by Mike
    if (s32Ret)
    {
        printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
        return s32Ret;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopViChn(VI_DEV ViDev, VI_CHN ViChn)
{
    HI_S32 s32Ret;
    s32Ret = HI_MPI_VI_DisableChn(ViChn);
    if (s32Ret)
    {
        printf("disable (%d,%d) fail,%x\n", ViDev, ViChn, s32Ret);
        return s32Ret;
    }
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartViByChn
 Description     : start vi. Just input how many channels you want to start.
                   It decide how many devices will be start. vi devices
                   begin from 0.
 Input           : s32ChnTotal   ** how many vi channels you want to start.
                   pstViDevAttr  ** attribute of vi device
                   pstViChnAttr  ** attribute of vi channel
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Used together with SAMPLE_StopViByChn().
                   All vi devices/channles must have the same attribute.
*****************************************************************************/
HI_S32 SAMPLE_StartViByChn(HI_S32 s32ChnTotal, VI_DEV_ATTR_S* pstViDevAttr, VI_CHN_ATTR_S* pstViChnAttr)
{
    HI_S32 i;
    VI_DEV ViDev,vi_start;
    HI_S32 s32ChnPerDev;
    HI_S32 s32DevTotal;
    HI_S32 ViChn;
    HI_S32 s32Ret;
    HI_S32 s32ChnCnt;
    HI_U32 u32SrcFrmRate;

    s32ChnPerDev = SampleVichnPerDev(pstViDevAttr->enIntfMode, pstViDevAttr->enWorkMode);
    s32DevTotal = (s32ChnTotal - 1) / s32ChnPerDev + 1;
    u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 25 : 30;

    s32ChnCnt = 0;
    vi_start = G_VIDEV_START;
    for (i = 0; i < s32DevTotal; i++)
    {
        ViDev = i;
        s32Ret = HI_MPI_VI_SetDevAttr(ViDev, pstViDevAttr);  //add by Mike
        if (s32Ret)
        {
            printf("set vi dev %d attr fail,%x\n", ViDev, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VI_EnableDev(ViDev);
        if (s32Ret)
        {
            printf("VI dev %d enable fail \n", ViDev);
            return s32Ret;
        }

        printf("enable vi dev %d ok\n", ViDev);

        for (ViChn = 0; ViChn < s32ChnPerDev; ViChn++)
        {
            s32Ret = HI_MPI_VI_SetChnAttr(ViChn, pstViChnAttr);  //add by Mike
            if (s32Ret)
            {
                printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
                return s32Ret;
            }

            s32Ret = HI_MPI_VI_EnableChn(ViChn);  //add by Mike
            if (s32Ret)
            {
                printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
                return s32Ret;
            }

            printf("enable vi(%d,%d) ok\n", ViDev, ViChn);

            /* return when all channel ok. */
            if(++s32ChnCnt >= s32ChnTotal)
            {
                return HI_SUCCESS;
            }
        }
    }
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StopViByChn
 Description     : stop vi. Just input how many channels you want to stop.
                   It decide how many devices will be stop. vi devices
                   begin from 0.
 Input           : s32ChnTotal  ** how many vi channels you want to stop.
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Used together with SAMPLE_StartViByChn().
                   All vi devices/channles must have the same attribute.
*****************************************************************************/
HI_S32 SAMPLE_StopViByChn(HI_S32 s32ChnTotal)
{
    VI_DEV ViDev,vi_start;
    HI_S32 s32ChnPerDev;
    HI_S32 s32DevTotal;
    HI_S32 ViChn;
    HI_S32 s32Ret;
    HI_S32 s32ChnCnt;

    s32ChnPerDev = SAMPLE_GetViChnPerDev(0);
    s32DevTotal = (s32ChnTotal - 1) / s32ChnPerDev + 1;

    s32ChnCnt = 0;
    vi_start = G_VIDEV_START;
    for (ViDev = vi_start; ViDev <vi_start+ s32DevTotal; ViDev++)
    {
        for (ViChn = 0; ViChn < s32ChnPerDev; ViChn++)
        {
            s32Ret = HI_MPI_VI_DisableChn(ViChn);  //add by Mike
            if (HI_SUCCESS != s32Ret)
            {
                printf("HI_MPI_VI_DisableChn(%d,%d) fail, err code: 0x%08x\n",
                    ViDev, ViChn, s32Ret);
            }

            /* return when all channel ok. */
		s32ChnCnt++;
        }

        //HI_MPI_VI_Disable(ViDev);          //add by Mike
        if(s32ChnCnt >= s32ChnTotal)
        {
            return HI_SUCCESS;
        }

    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartViByDev
 Description     : start vi device and its channel.
 Input           : s32ChnTotal   ** how many channel start on this device
                   ViDev         ** which vi device you want to start
                   pstViDevAttr  ** attribute of this device
                   pstViChnAttr  ** attribute of all channels on this device
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Used together with SAMPLE_StopViByDev().
*****************************************************************************/
HI_S32 SAMPLE_StartViByDev(HI_S32 s32ChnTotal, VI_DEV ViDev, VI_DEV_ATTR_S* pstViDevAttr, VI_CHN_ATTR_S* pstViChnAttr)  //add by Mike
{
    HI_S32 ViChn;
    HI_S32 s32Ret;

    s32Ret = HI_MPI_VI_SetDevAttr(ViDev, pstViDevAttr);
    if (s32Ret)
    {
        printf("set vi dev %d attr fail,%x\n", ViDev, s32Ret);
        return HI_FAILURE;
    }

    //s32Ret = HI_MPI_VI_Enable(ViDev);    //add by Mike
    //if (s32Ret)
    //{
    //    printf("VI dev %d enable fail \n", ViDev);
    //    return s32Ret;
    //}

    printf("enable vi dev %d ok\n", ViDev);

    for (ViChn = 0; (ViChn < s32ChnTotal); ViChn++)
    {
        s32Ret = HI_MPI_VI_SetChnAttr(ViChn, pstViChnAttr);  //add by Mike
        if (s32Ret)
        {
            printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
            return s32Ret;
        }

        s32Ret = HI_MPI_VI_EnableChn(ViChn);  //add by Mike
        if (s32Ret)
        {
            printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
            return s32Ret;
        }

        printf("enable vi(%d,%d) ok\n", ViDev, ViChn);
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StopViByDev
 Description     : Stop vi device and all channls on this device.
 Input           : ViDev        **
                   s32ChnTotal  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Used together with SAMPLE_StartViByDev().
*****************************************************************************/
HI_S32 SAMPLE_StopViByDev(HI_S32 s32ChnTotal, VI_DEV ViDev)
{
    HI_S32 ViChn;
    HI_S32 s32Ret;

    for (ViChn = 0; ViChn < s32ChnTotal; ViChn++)
    {
        s32Ret = HI_MPI_VI_DisableChn(ViChn);  //add by Mike
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VI_DisableChn(%d, %d) fail, err no: 0x%08x.\n",
                ViDev, ViChn, s32Ret);
            return HI_FAILURE;
        }

    }

    //s32Ret = HI_MPI_VI_Disable(ViDev);    //add by Mike
    //if ( HI_SUCCESS != s32Ret )
    //{
    //    printf("HI_MPI_VI_Disable(%d) fail, err no: 0x%08x.\n",
    //        ViDev, s32Ret);
    //    return HI_FAILURE;
    //}

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVoDevice
 Description     : Start vo device as you specified.
 Input           : VoDev       **
                   pstDevAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_StartVoDevice(VO_DEV VoDev, VO_PUB_ATTR_S* pstDevAttr)
{
    HI_S32 ret;

	/*because we will change vo device attribution,
	  so we diable vo device first*/
	ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_Disable fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    ret = HI_MPI_VO_SetPubAttr(VoDev, pstDevAttr);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_SetPubAttr fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    ret = HI_MPI_VO_Enable(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_Enable fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_StartVoVideoLayer(VO_DEV VoDev, VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    HI_S32 ret;

    /* set public attr of VO*/
    ret = HI_MPI_VO_SetVideoLayerAttr(VoDev, pstVideoLayerAttr);
    if (HI_SUCCESS != ret)
    {
        printf("set video layer of dev %u failed %#x!\n", VoDev, ret);
        return HI_FAILURE;
    }

    /* enable VO device*/
    ret = HI_MPI_VO_EnableVideoLayer(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("enable video layer of dev %d failed with %#x !\n", VoDev, ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_SetVoChnMScreen(VO_DEV VoDev, HI_U32 u32ChnCnt, HI_U32 u32Width, HI_U32 u32Height)
{
    HI_U32 i, div, w, h, ret;
    VO_CHN_ATTR_S stChnAttr;

    /* If display 32 vo channels, should use 36-screen split. */
    u32ChnCnt = (u32ChnCnt == VO_MAX_CHN_NUM) ? 36 : u32ChnCnt;

    div = sqrt(u32ChnCnt);
    w = (u32Width / div);
    h = (u32Height / div);
    
    for (i = 0; i < u32ChnCnt; i++)
    {
        if (i >= VO_MAX_CHN_NUM)
        {
            break;
        }

        stChnAttr.u32Priority = 0;
        stChnAttr.bDeflicker  = HI_FALSE;
        stChnAttr.stRect.s32X = w * (i % div);
        stChnAttr.stRect.s32Y = h * (i / div);
        stChnAttr.stRect.u32Width  = w;
        stChnAttr.stRect.u32Height = h;

        if (stChnAttr.stRect.s32X % 2 != 0)
        {
            stChnAttr.stRect.s32X++;
        }

        if (stChnAttr.stRect.s32Y % 2 != 0)
        {
            stChnAttr.stRect.s32Y++;
        }

        if (stChnAttr.stRect.u32Width % 2 != 0)
        {
            stChnAttr.stRect.u32Width++;
        }

        if (stChnAttr.stRect.u32Height % 2 != 0)
        {
            stChnAttr.stRect.u32Height++;
        }

        ret = HI_MPI_VO_SetChnAttr(VoDev, i, &stChnAttr);
        if (ret != HI_SUCCESS)
        {
            printf("In %s set channel %d attr failed with %#x!\n", __FUNCTION__, i, ret);
            return ret;
        }
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVo
 Description     : Start VO to output CVBS signal
 Input           : VoDev       **  SD, AD or HD
                   s32ChnTotal **  how many channel display on screen.
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_StartVo(HI_S32                 s32ChnTotal,
                            VO_DEV                 VoDev,
                            VO_PUB_ATTR_S*         pstVoDevAttr,
                            VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    HI_S32 s32Ret;
    HI_U32 u32Width;
    HI_U32 u32Height;
    VO_CHN VoChn;

    s32Ret = SAMPLE_StartVoDevice(VoDev, pstVoDevAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* set vo video layer attribute and start video layer. */
    s32Ret = SAMPLE_StartVoVideoLayer(VoDev, pstVideoLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* set vo channel attribute and start each channel. */
    u32Width  = pstVideoLayerAttr->stImageSize.u32Width;
    u32Height = pstVideoLayerAttr->stImageSize.u32Height;

    if (s32ChnTotal == 1)
    {
        ;
    }
    else if (s32ChnTotal <= 4)
    {
        s32ChnTotal = 4;
    }
    else if (s32ChnTotal <= 9)
    {
        s32ChnTotal = 9;
    }
    else if (s32ChnTotal <= 16)
    {
        s32ChnTotal = 16;
    }
    else if (s32ChnTotal <= 36)
    {
        /* 36-screen split is support. But only 32 vo channels display. */
        s32ChnTotal = VO_MAX_CHN_NUM;
    }
    else
    {
        printf("too many vo channels!\n");
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_SetVoChnMScreen(VoDev, s32ChnTotal, u32Width, u32Height);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    for (VoChn = 0; VoChn < s32ChnTotal; VoChn++)
    {
        s32Ret = HI_MPI_VO_EnableChn(VoDev, VoChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_EnableChn(%d, %d) failed, err code:0x%08x\n\n",
                VoDev, VoChn, s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopVo(HI_S32 s32ChnTotal, VO_DEV VoDev)
{
    VO_CHN VoChn;
    HI_S32 s32Ret;

    for (VoChn = 0; VoChn < s32ChnTotal; VoChn++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoDev, VoChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_DisableChn(%d, %d) fail, err code: 0x%08x.\n",
                VoDev, VoChn, s32Ret);
            return HI_FAILURE;
        }
    }
    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_DisableVideoLayer(%d) fail, err code: 0x%08x.\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Disable(%d) fail, err code: 0x%08x.\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}
/*****************************************************************************
 Prototype       : SAMPLE_ViBindVo
 Description     : Bind VI to VO sequently, i.e, VI channe is same with VO channel
                   binded.
 Input           : u32ViChnCnt  **
                   VoDev        **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : frames captured by different vi device may display on one vo device.
*****************************************************************************/
HI_S32 SAMPLE_ViBindVo(HI_S32 s32ViChnTotal, VO_DEV VoDev)
{
    HI_S32 ViChnCnt;
    HI_S32 s32ViChnPerDev;
    VI_DEV ViDev;
    VO_CHN VoChn;
    VI_CHN ViChn;
    //HI_S32 s32Ret;

    s32ViChnPerDev = SAMPLE_GetViChnPerDev(0);

    /* Bind VI channel to VO channel */
    ViDev = G_VIDEV_START;
    VoChn = 0;
    ViChnCnt = 0;
    while (1)
    {
        for (ViChn = 0; ViChn < s32ViChnPerDev; ViChn++, VoChn++)
        {
            //s32Ret = HI_MPI_VI_BindOutput(ViDev, ViChn, VoDev, VoChn);  //add by Mike
            //if (0 != s32Ret)
            //{
            //    printf("bind vi2vo failed, vi(%d,%d),vo(%d,%d)!\n",
            //           ViDev, ViChn, VoDev, VoChn);
            //    return s32Ret;
            //}

            ViChnCnt++;
            if (ViChnCnt >= s32ViChnTotal)
            {
                return HI_SUCCESS;
            }
        }
#ifdef hi3515
        ViDev += 2;
#else
        ViDev ++;
#endif
    }

    return HI_SUCCESS;
}




/***************Level 2 TEST CODE************************/

/*****************************************************************************
 Prototype       : SAMPLE_StartVi_SD
 Description     : start vi to input standard-definition video.
 Input           : s32ViChnTotal  **
                   enPicSize      **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_StartVi_SD(HI_S32 s32ViChnTotal, SAMPLE_VI_DEV_TYPE_E enViDevType, PIC_SIZE_E enDesPicSize)
{
    VI_DEV_ATTR_S stViDevAttr;
    VI_CHN_ATTR_S stViChnAttr;
    HI_S32 s32Ret;
    SIZE_S stDesSize;
    HI_S32 i;
    VI_DEV ViDev;
    HI_S32 ViChn;
    HI_S32 s32ChnCnt=0;

    SAMPLE_VI_CHN_SET_E enViChnSet = VI_CHN_SET_NORMAL;
    for (ViDev = 0; ViDev < 4; ViDev++)
    {
        ViDev = ViDev * 2;
        switch (enViDevType)
        {
            case VI_DEV_BT656_D1_1MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_1MUX,sizeof(stViDevAttr));
                SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
                #ifndef Board_Test_B
                s32Ret = SAMPLE_TW2865_CfgV(gs_enViNorm,stViDevAttr.enWorkMode);
                if (s32Ret != HI_SUCCESS)
                {
                    printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                       __FUNCTION__,  s32Ret);
                    return HI_FAILURE;
                }
                #endif
                break;
            case VI_DEV_BT656_D1_4MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_4MUX,sizeof(stViDevAttr));
                #ifndef Board_Test_B
                s32Ret = SAMPLE_TW2865_CfgV(gs_enViNorm,stViDevAttr.enWorkMode);
                if (s32Ret != HI_SUCCESS)
                {
                    printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                       __FUNCTION__,  s32Ret);
                    return HI_FAILURE;
                }
                #endif
                break;

            case VI_DEV_BT656_960H_1MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_1MUX,sizeof(stViDevAttr));
                SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
                #ifndef Board_Test_B
                s32Ret = SAMPLE_TW2960_CfgV(gs_enViNorm,stViDevAttr.enWorkMode);
                if (s32Ret != HI_SUCCESS)
                {
                    printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                       __FUNCTION__,  s32Ret);
                    return HI_FAILURE;
                }
                #endif
                break;
            case VI_DEV_BT656_960H_4MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_4MUX,sizeof(stViDevAttr));
                SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
                #ifndef Board_Test_B
                s32Ret = SAMPLE_TW2960_CfgV(gs_enViNorm,stViDevAttr.enWorkMode);
                if (s32Ret != HI_SUCCESS)
                {
                    printf("%s: SAMPLE_TW2865_CfgV failed with %#x!\n",\
                       __FUNCTION__,  s32Ret);
                    return HI_FAILURE;
                }
                #endif
                break;

            case VI_DEV_720P_HD_1MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_7441_BT1120_720P,sizeof(stViDevAttr));
                SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
                break;
            case VI_DEV_1080P_HD_1MUX:
                memcpy(&stViDevAttr,&DEV_ATTR_7441_BT1120_1080P,sizeof(stViDevAttr));
                SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
                break;
            default:
                printf("vi input type[%d] is invalid!\n", enViDevType);
                return HI_FAILURE;
        }
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enViNorm, enDesPicSize, &stDesSize);
    if (s32Ret)
    {
        printf("%s(%d): get picture size[%d] failed!,return value is %d.\n",
            __FUNCTION__,__LINE__, enDesPicSize, s32Ret);
        return HI_FAILURE;
    }

    /* step  5: config & start vicap dev */
    stViChnAttr.stCapRect.s32X = 0;
    stViChnAttr.stCapRect.s32Y = 0;
    stViChnAttr.stDestSize.u32Width  = stDesSize.u32Width;
    stViChnAttr.stDestSize.u32Height = stDesSize.u32Height;
    stViChnAttr.stCapRect.u32Width   = stViChnAttr.stDestSize.u32Width;
    stViChnAttr.stCapRect.u32Height  = stViChnAttr.stDestSize.u32Height;
    stViChnAttr.enCapSel = VI_CAPSEL_BOTH;
    /* to show scale. this is a sample only, we want to show dist_size = D1 onley */
    stViChnAttr.stDestSize.u32Width = stDesSize.u32Width;
    stViChnAttr.stDestSize.u32Height = stDesSize.u32Height;
    stViChnAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;   /* sp420 or sp422 */
    stViChnAttr.bMirror = (VI_CHN_SET_MIRROR == enViChnSet)?HI_TRUE:HI_FALSE;
    stViChnAttr.bFlip = (VI_CHN_SET_FILP == enViChnSet)?HI_TRUE:HI_FALSE;

    stViChnAttr.bChromaResample = HI_FALSE;
    stViChnAttr.s32SrcFrameRate = -1;
    stViChnAttr.s32FrameRate = -1;


    for (i = 0; i < 4; i++)
    {
        ViDev = i * 2;
        SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
        s32Ret = HI_MPI_VI_SetDevAttr(ViDev, &stViDevAttr);  //add by Mike
        if (s32Ret)
        {
            printf("set vi dev %d attr fail,%x\n", ViDev, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VI_EnableDev(ViDev);
        if (s32Ret)
        {
            printf("VI dev %d enable fail \n", ViDev);
            return s32Ret;
        }

        printf("enable vi dev %d ok\n", ViDev);
    }

        for (ViChn = 0; ViChn < 16; ViChn++)
        {
            s32Ret = HI_MPI_VI_SetChnAttr(ViChn, &stViChnAttr);  //add by Mike
            if (s32Ret)
            {
                printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
                return s32Ret;
            }

            s32Ret = HI_MPI_VI_EnableChn(ViChn);  //add by Mike
            if (s32Ret)
            {
                printf("set vi(%d,%d) attr fail,%x\n", ViDev, ViChn, s32Ret);
                return s32Ret;
            }

            printf("enable vi(%d,%d) ok\n", ViDev, ViChn);

            /* return when all channel ok. */
            if(++s32ChnCnt >= s32ViChnTotal)
            {
                return HI_SUCCESS;
            }

        }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVo_SD
 Description     : start vo to display standard-definition video.
 Input           : s32VoChnTotal  **
                   VoDev          **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Using different vo device, you can display video on SDTV or VGA.
*****************************************************************************/
HI_S32 SAMPLE_StartVo_SD(HI_S32 s32VoChnTotal, VO_DEV VoDev)
{
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;
    HI_U32 u32Width;
    HI_U32 u32Height;
    HI_U32 u32DisplayRate = -1;
    HI_S32 s32Ret;

    switch (VoDev)
    {
        case VO_DEV_HD:
            stVoDevAttr.enIntfType = VO_INTF_VGA;
            stVoDevAttr.enIntfSync = VO_OUTPUT_800x600_60;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        case VO_DEV_AD:
            stVoDevAttr.enIntfType = VO_INTF_CVBS;
            stVoDevAttr.enIntfSync = gs_enSDTvMode;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        case VO_DEV_SD:
            stVoDevAttr.enIntfType = VO_INTF_CVBS;
            stVoDevAttr.enIntfSync = gs_enSDTvMode;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        default:
            return HI_FAILURE;
    }

    u32Width = 720;
    u32Height = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 576 : 480;
    stVideoLayerAttr.stDispRect.s32X = 0;
    stVideoLayerAttr.stDispRect.s32Y = 0;
    stVideoLayerAttr.stDispRect.u32Width   = u32Width;
    stVideoLayerAttr.stDispRect.u32Height  = u32Height;
    stVideoLayerAttr.stImageSize.u32Width  = u32Width;
    stVideoLayerAttr.stImageSize.u32Height = u32Height;
    stVideoLayerAttr.u32DispFrmRt = u32DisplayRate;
    stVideoLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    //stVideoLayerAttr.s32PiPChn = VO_DEFAULT_CHN;     //add by Mike

    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);
    if(HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartViVo_SD
 Description     : start vio (i.e. preview) . input and output are both standart-
                   definition.
 Input           : s32ChnTotal  **
                   enPicSize    **
                   VoDev        **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :

*****************************************************************************/
HI_S32 SAMPLE_StartViVo_SD(HI_S32 s32ChnTotal, PIC_SIZE_E enViSize, VO_DEV VoDev)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    VO_CHN VoChn;
    HI_S32 s32ChnCnt;
    HI_S32 s32ViChnPerDev;
    //HI_S32 s32Ret;

    s32ViChnPerDev = 4;
    ViDev = G_VIDEV_START;
    ViChn = 0;
    VoChn = 0;

    //SAMPLE_StartVi_SD(s32ChnTotal, enViSize);
    SAMPLE_StartVo_SD(s32ChnTotal, VoDev);

    s32ChnCnt = 0;
    while(s32ChnTotal--)
    {
        //s32Ret = HI_MPI_VI_BindOutput(ViDev, ViChn, VoDev, VoChn);    //add by Mike
        //if (HI_SUCCESS != s32Ret)
        //{
        //    printf("bind Vi(%d,%d) to Vo(%d,%d) fail! \n",
        //        ViDev, ViChn, VoDev, VoChn);
        //    return HI_FAILURE;
        //}
        ViChn++;
        VoChn++;
        if (++s32ChnCnt == s32ViChnPerDev)
        {
            s32ChnCnt = 0;
            ViChn = 0;
            #ifdef hi3515
            ViDev+=2;
            #else
            ViDev++;
            #endif
        }
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopViVo_SD(HI_S32 s32ChnTotal, VO_DEV VoDev)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    VO_CHN VoChn;
    HI_S32 s32ChnCnt;
    HI_S32 s32ViChnPerDev;
    //HI_S32 s32Ret;

    s32ViChnPerDev = 4;
    ViDev = G_VIDEV_START;
    ViChn = 0;
    VoChn = 0;

    SAMPLE_StopViByChn(s32ChnTotal);
    SAMPLE_StopVo(s32ChnTotal, VoDev);

    s32ChnCnt = 0;
    while(s32ChnTotal--)
    {
        //s32Ret = HI_MPI_VI_UnBindOutput(ViDev, ViChn, VoDev, VoChn);  //add by Mike
        //if (HI_SUCCESS != s32Ret)
        //{
        //    printf("bind Vi(%d,%d) to Vo(%d,%d) fail! \n",
        //        ViDev, ViChn, VoDev, VoChn);
        //    return HI_FAILURE;
        //}
        ViChn++;
        VoChn++;
        if (++s32ChnCnt == s32ViChnPerDev)
        {
            s32ChnCnt = 0;
            ViChn = 0;
            ViDev++;
        }
    }
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StopAllVi
 Description     : Stop all vi
 Input           : HI_VOID  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : simplified method to stop vi.
*****************************************************************************/
HI_S32 SAMPLE_StopAllVi(HI_VOID)
{
    HI_S32 i;

    for (i = 0; i < 16; i++)
    {
        HI_MPI_VI_DisableChn(i);
    }

    for (i = 0; i < 4; i++)
    {
        HI_MPI_VI_DisableDev(i*2);
    }

    return 0;
}

HI_S32 SAMPLE_StopAllVo(HI_VOID)
{
    VO_DEV VoDev;
    VO_CHN VoChn;

    for (VoDev = 0; VoDev < VO_MAX_DEV_NUM; VoDev++)
    {
        for (VoChn = 0; VoChn < VO_MAX_CHN_NUM; VoChn++)
        {
            HI_MPI_VO_DisableChn(VoDev, VoChn);
        }

        HI_MPI_VO_DisableVideoLayer(VoDev);
        HI_MPI_VO_Disable(VoDev);
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VoPicSwitch(VO_DEV VoDev, HI_U32 u32VoPicDiv)
{
    VO_CHN VoChn;
    VO_CHN_ATTR_S VoChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_S32 s32Ret;
    HI_S32 i, div, u32ScreemDiv, u32PicWidth, u32PicHeight;

    u32ScreemDiv = u32VoPicDiv;

    s32Ret = HI_MPI_VO_SetAttrBegin(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_SetAttrBegin(%d) errcode: 0x%08x\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }
#if 1
    for (i = 0; i < VO_MAX_CHN_NUM; i++)
    {
        if (i < 16)
            HI_MPI_VO_ChnHide(VoDev, i);
    }
#endif
    div = sqrt(u32ScreemDiv);

    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoDev, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_GetVideoLayerAttr(%d) errcode: 0x%08x\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }
    u32PicWidth  = stLayerAttr.stDispRect.u32Width / div;
    u32PicHeight = stLayerAttr.stDispRect.u32Height / div;

    for (i = 0; i < u32ScreemDiv; i++)
    {
        VoChn = i;
        VoChnAttr.stRect.s32X = (i % div) * u32PicWidth;
        VoChnAttr.stRect.s32Y = (i / div) * u32PicHeight;
        VoChnAttr.stRect.u32Width  = u32PicWidth;
        VoChnAttr.stRect.u32Height = u32PicHeight;
        VoChnAttr.u32Priority = 1;
        //VoChnAttr.bZoomEnable = HI_TRUE;  //add by Mike
        VoChnAttr.bDeflicker  = HI_FALSE;
        if (0 != HI_MPI_VO_SetChnAttr(VoDev, VoChn, &VoChnAttr))
        {
            printf("set VO Chn %d attribute(%d,%d,%d,%d) failed !\n",
                   VoChn, VoChnAttr.stRect.s32X, VoChnAttr.stRect.s32Y,
                   VoChnAttr.stRect.u32Width, VoChnAttr.stRect.u32Height);
            return -1;
        }
#if 1
        if (0 != HI_MPI_VO_ChnShow(VoDev, VoChn))
        {
            return -1;
        }
#endif
    }

    if (0 != HI_MPI_VO_SetAttrEnd(VoDev))
    {
        return -1;
    }

    return 0;
}

HI_S32 SAMPLE_GetJpegeCfg(PIC_SIZE_E enPicSize, VENC_ATTR_JPEG_S *pstJpegeAttr)
{
    VENC_ATTR_JPEG_S stJpegAttr;

    if (PIC_D1 == enPicSize)
    {
        stJpegAttr.u32PicWidth          = 704;
        stJpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
    }
    else if (PIC_CIF == enPicSize)
    {
        stJpegAttr.u32PicWidth          = 352;
        stJpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }
    stJpegAttr.u32MaxPicWidth = stJpegAttr.u32PicWidth;
    stJpegAttr.u32MaxPicHeight= stJpegAttr.u32PicHeight;

    stJpegAttr.u32BufSize   = stJpegAttr.u32PicWidth * stJpegAttr.u32PicHeight * 2;
    stJpegAttr.bVIField     = HI_TRUE;
    stJpegAttr.bByFrame     = HI_TRUE;
    //stJpegAttr.u32MCUPerECS = 0;             //add by Mike
    stJpegAttr.u32Priority  = 0;
    //stJpegAttr.u32ImageQuality = 3;

    memcpy(pstJpegeAttr, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));

    return 0;
}

HI_S32 SAMPLE_GetMjpegeCfg(PIC_SIZE_E enPicSize, HI_BOOL bMainStream,
        VENC_ATTR_MJPEG_S *pstMjpegeAttr)
{
    VENC_ATTR_MJPEG_S stMjpegAttr;

    if (PIC_D1 == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 704;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
        //stMjpegAttr.u32TargetBitrate     = 8192;   //add by Mike
    }
    else if (PIC_CIF == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 352;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stMjpegAttr.u32TargetBitrate     = 4096;   //add by Mike
    }
    else if (PIC_QCIF == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 176;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?144:120;;
        //stMjpegAttr.u32TargetBitrate     = 2048;   //add by Mike
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }

    stMjpegAttr.bMainStream             = bMainStream;
    stMjpegAttr.bByFrame                = HI_TRUE;
    stMjpegAttr.bVIField                = HI_TRUE;
    //stMjpegAttr.u32ViFramerate          = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?25:30;    //add by Mike
    //stMjpegAttr.u32TargetFramerate      = stMjpegAttr.u32ViFramerate;
    //stMjpegAttr.u32MCUPerECS            = 0;
    stMjpegAttr.u32BufSize              = stMjpegAttr.u32PicWidth * stMjpegAttr.u32PicHeight * 2;

    memcpy(pstMjpegeAttr, &stMjpegAttr, sizeof(VENC_ATTR_MJPEG_S));

    return 0;
}

HI_S32 SAMPLE_GetH264eCfg(PIC_SIZE_E enPicSize, HI_BOOL bMainStream,
        VENC_ATTR_H264_S *pstH264eAttr)
{
    VENC_ATTR_H264_S stH264Attr;

    if (PIC_D1 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 704;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
        //stH264Attr.u32Bitrate           = 1024;             //add by Mike
    }
    else if (PIC_HD1 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 704;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stH264Attr.u32Bitrate           = 1024;            //add by Mike
    }
    else if (PIC_CIF == enPicSize)
    {
        stH264Attr.u32PicWidth          = 352;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stH264Attr.u32Bitrate           = 512;             //add by Mike
    }
    else if (PIC_QCIF == enPicSize)
    {
        stH264Attr.u32PicWidth          = 176;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?144:120;
        //stH264Attr.u32Bitrate           = 256;                //add by Mike
    }
    else if (PIC_VGA == enPicSize)
    {
        stH264Attr.u32PicWidth          = 640;
        stH264Attr.u32PicHeight         = 480;
        //stH264Attr.u32Bitrate           = 1024;             //add by Mike
    }
    else if (PIC_QVGA == enPicSize)
    {
        stH264Attr.u32PicWidth          = 320;
        stH264Attr.u32PicHeight         = 240;
        //stH264Attr.u32Bitrate           = 512;                 //add by Mike
    }
    else if (PIC_HD720 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 1280;
        stH264Attr.u32PicHeight         = 720;
        //stH264Attr.u32Bitrate           = 2048;                //add by Mike
    }
    else if (PIC_HD1080 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 1920;
        stH264Attr.u32PicHeight         = 1072;
        //stH264Attr.u32Bitrate           = 4000;                //add by Mike
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }

    stH264Attr.bMainStream          = bMainStream;
    stH264Attr.bByFrame             = HI_TRUE;
    //stH264Attr.enRcMode             = RC_MODE_CBR;             //add by Mike
    stH264Attr.bField               = HI_FALSE;
    stH264Attr.bVIField             = HI_TRUE;
    //stH264Attr.u32ViFramerate       = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?25:30;
    //stH264Attr.u32TargetFramerate   = stH264Attr.u32ViFramerate;
    //stH264Attr.u32Gop               = 100;
    //stH264Attr.u32MaxDelay          = 100;
    //stH264Attr.u32PicLevel          = 0;
    stH264Attr.u32Priority          = 0;
    stH264Attr.u32BufSize           = stH264Attr.u32PicWidth * stH264Attr.u32PicHeight * 2;

    memcpy(pstH264eAttr, &stH264Attr, sizeof(VENC_ATTR_H264_S));

    return 0;
}

HI_S32 SAMPlE_GetGrpViMap(VENC_GRP VeGrp, VI_DEV *pViDev, VI_CHN *pViChn)
{
    HI_S32 s32ViChnPerDev;

    s32ViChnPerDev = SAMPLE_GetViChnPerDev(0);
    *pViDev = VeGrp / s32ViChnPerDev;
    *pViChn = VeGrp % s32ViChnPerDev;
    return 0;
}

HI_S32 SAMPLE_StartOneVenc(VENC_GRP VeGrp, VI_DEV ViDev, VI_CHN ViChn,
    PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, HI_S32 s32FrmRate)
{
    HI_S32 s32Ret;
    VENC_CHN VeChn;
    VENC_ATTR_H264_S stH264eAttr;
    VENC_ATTR_MJPEG_S stMjpegAttr;
    VENC_CHN_ATTR_S stVencAttr;

    if (PT_H264 == enType)
    {
        /* you should get config by your method */
        SAMPLE_GetH264eCfg(enSize, HI_TRUE, &stH264eAttr);
        //stH264eAttr.u32TargetFramerate = s32FrmRate;             //add by Mike
    }
    else if (PT_MJPEG == enType)
    {
        SAMPLE_GetMjpegeCfg(enSize, HI_TRUE, &stMjpegAttr);
        //stMjpegAttr.u32TargetFramerate = s32FrmRate;              //add by Mike
    }
    else
    {
        printf("venc not support this payload type :%d\n", enType);
        return -1;
    }

    s32Ret = HI_MPI_VENC_CreateGroup(VeGrp);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateGroup(%d) err 0x%x\n", VeGrp, s32Ret);
        return HI_FAILURE;
    }

    //s32Ret = HI_MPI_VENC_BindInput(VeGrp, ViDev, ViChn);  //add by Mike
    //if (s32Ret != HI_SUCCESS)
    //{
    //    printf("HI_MPI_VENC_BindInput err 0x%x\n", s32Ret);
    //    return HI_FAILURE;
    //}
    //printf("grp %d bind vi(%d,%d) ok\n", VeGrp, ViDev, ViChn);

    VeChn = VeGrp;
    //stVencAttr.enType = enType;                //add by Mike
    //if (PT_H264 == stVencAttr.enType)
    //{
    //    stVencAttr.pValue = &stH264eAttr;
    //}
    //else
    //{
    //    stVencAttr.pValue = &stMjpegAttr;
    //}                                        //add by Mike
    s32Ret = HI_MPI_VENC_CreateChn(VeChn, &stVencAttr);        //add by Mike
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateChn(%d) err 0x%x\n", VeChn, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_RegisterChn(VeGrp, VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_RegisterChn err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_StartRecvPic(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_StartRecvPic err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }
    printf("venc chn%d start ok\n", VeChn);

    return HI_SUCCESS;
}



/******************************************************************************
* funciton : Start venc stream mode (h264, mjpeg)
* note      : rate control parameter need adjust, according your case.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_Start(VENC_GRP VencGrp,VENC_CHN VencChn, PAYLOAD_TYPE_E enType, VIDEO_NORM_E enNorm, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_H264_S stH264Attr;
    VENC_ATTR_H264_CBR_S    stH264Cbr;
    VENC_ATTR_H264_VBR_S    stH264Vbr;
    VENC_ATTR_H264_FIXQP_S  stH264FixQp;
    VENC_ATTR_MJPEG_S stMjpegAttr;
    VENC_ATTR_MJPEG_FIXQP_S stMjpegeFixQp;
    VENC_ATTR_JPEG_S stJpegAttr;
    SIZE_S stPicSize;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enNorm, enSize, &stPicSize);
     if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }
    /******************************************
     step 1: Greate Venc Group
    ******************************************/
    s32Ret = HI_MPI_VENC_CreateGroup(VencGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateGroup[%d] failed with %#x!\n",\
                 VencGrp, s32Ret);
        return HI_FAILURE;
    }

    /******************************************
     step 2:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVeAttr.enType = enType;
    switch(enType)
    {
        case PT_H264:
        {
            stH264Attr.u32MaxPicWidth = stPicSize.u32Width;
            stH264Attr.u32MaxPicHeight = stPicSize.u32Height;
            stH264Attr.u32PicWidth = stPicSize.u32Width;/*the picture width*/
            stH264Attr.u32PicHeight = stPicSize.u32Height;/*the picture height*/
            stH264Attr.u32BufSize  = stPicSize.u32Width * stPicSize.u32Height * 2;/*stream buffer size*/
            stH264Attr.u32Profile  = 0;/*0: baseline; 1:MP; 2:HP   ? */
            stH264Attr.bByFrame = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
            stH264Attr.bField = HI_FALSE;  /* surpport frame code only for hi3516, bfield = HI_FALSE */
            stH264Attr.bMainStream = HI_TRUE; /* surpport main stream only for hi3516, bMainStream = HI_TRUE */
            stH264Attr.u32Priority = 0; /*channels precedence level. invalidate for hi3516*/
            stH264Attr.bVIField = HI_FALSE;/*the sign of the VI picture is field or frame. Invalidate for hi3516*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrH264e, &stH264Attr, sizeof(VENC_ATTR_H264_S));

            if(SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop            = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Cbr.u32StatTime       = 1; /* stream rate statics time(s) */
                stH264Cbr.u32ViFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* input (vi) frame rate */
                stH264Cbr.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* target frame rate */
                switch (enSize)
                {
                  case PIC_QCIF:
                       stH264Cbr.u32BitRate = 256; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:

                       stH264Cbr.u32BitRate = 512;
                       break;

                  case PIC_D1:
                  case PIC_VGA:    /* 640 * 480 */
                       stH264Cbr.u32BitRate = 1024*2;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                       stH264Cbr.u32BitRate = 1024*3;
                       break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                       stH264Cbr.u32BitRate = 1024*6;
                       break;
                  default :
                       stH264Cbr.u32BitRate = 1024*4;
                       break;
                }

                stH264Cbr.u32FluctuateLevel = 0; /* average bit rate */
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Cbr, &stH264Cbr, sizeof(VENC_ATTR_H264_CBR_S));
            }
            else if (SAMPLE_RC_FIXQP == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32IQp = 20;
                stH264FixQp.u32PQp = 23;
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264FixQp, &stH264FixQp,sizeof(VENC_ATTR_H264_FIXQP_S));
            }
            else if (SAMPLE_RC_VBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32StatTime = 1;
                stH264Vbr.u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32MinQp = 24;
                stH264Vbr.u32MaxQp = 32;
                switch (enSize)
                {
                  case PIC_QCIF:
                       stH264Vbr.u32MaxBitRate= 256*3; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                       stH264Vbr.u32MaxBitRate = 512*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:    /* 640 * 480 */
                       stH264Vbr.u32MaxBitRate = 1024*2*3;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                       stH264Vbr.u32MaxBitRate = 1024*3*3;
                       break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                       stH264Vbr.u32MaxBitRate = 1024*6*3;
                       break;
                  default :
                       stH264Vbr.u32MaxBitRate = 1024*4*3;
                       break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Vbr, &stH264Vbr, sizeof(VENC_ATTR_H264_VBR_S));
            }
            else
            {
                return HI_FAILURE;
            }
        }
        break;

        case PT_MJPEG:
        {
            stMjpegAttr.u32MaxPicWidth = stPicSize.u32Width;
            stMjpegAttr.u32MaxPicHeight = stPicSize.u32Height;
            stMjpegAttr.u32PicWidth = stPicSize.u32Width;
            stMjpegAttr.u32PicHeight = stPicSize.u32Height;
            stMjpegAttr.u32BufSize = stPicSize.u32Width * stPicSize.u32Height * 2;
            stMjpegAttr.bByFrame = HI_TRUE;  /*get stream mode is field mode  or frame mode*/
            stMjpegAttr.bMainStream = HI_TRUE;  /*main stream or minor stream types?*/
            stMjpegAttr.bVIField = HI_FALSE;  /*the sign of the VI picture is field or frame?*/
            stMjpegAttr.u32Priority = 0;/*channels precedence level*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrMjpeg, &stMjpegAttr, sizeof(VENC_ATTR_MJPEG_S));

            if(SAMPLE_RC_FIXQP == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor        = 90;
                stMjpegeFixQp.u32ViFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stMjpegeFixQp.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                memcpy(&stVencChnAttr.stRcAttr.stAttrMjpegeFixQp, &stMjpegeFixQp,
                       sizeof(VENC_ATTR_MJPEG_FIXQP_S));
            }
            else if (SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32StatTime       = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32ViFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.fr32TargetFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32FluctuateLevel = 0;
                switch (enSize)
                {
                  case PIC_QCIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 384*3; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 768*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:    /* 640 * 480 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*3*3;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*5*3;
                       break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*10*3;
                       break;
                  default :
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*7*3;
                       break;
                }
            }
            else if (SAMPLE_RC_VBR == enRcMode)
            {
            #if 0
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32StatTime = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.fr32TargetFrmRate = 5;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MinQfactor = 50;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxQfactor = 95;
                switch (enSize)
                {
                  case PIC_QCIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate= 256*3; /* average bit rate */
                       break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 512*3;
                       break;
                  case PIC_D1:
                  case PIC_VGA:    /* 640 * 480 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*2*3;
                       break;
                  case PIC_HD720:   /* 1280 * 720 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*3*3;
                       break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*6*3;
                       break;
                  default :
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*4*3;
                       break;
                }
            #endif
                SAMPLE_PRT("cann't support VBR mode in this version(MJPEG)!\n");
                return HI_FAILURE;

            }
            else
            {
                SAMPLE_PRT("cann't support other mode in this version!\n");

                return HI_FAILURE;
            }
        }
        break;

        case PT_JPEG:
            stJpegAttr.u32MaxPicWidth = stPicSize.u32Width;
            stJpegAttr.u32MaxPicHeight = stPicSize.u32Height;
            stJpegAttr.u32PicWidth  = stPicSize.u32Width;
            stJpegAttr.u32PicHeight = stPicSize.u32Height;
            stJpegAttr.u32BufSize = stPicSize.u32Width * stPicSize.u32Height * 2;
            stJpegAttr.bByFrame = HI_TRUE;/*get stream mode is field mode  or frame mode*/
            stJpegAttr.bVIField = HI_FALSE;/*the sign of the VI picture is field or frame?*/
            stJpegAttr.u32Priority = 0;/*channels precedence level*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrJpeg, &stJpegAttr, sizeof(VENC_ATTR_MJPEG_S));
            break;
        default:
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] faild with %#x!\n",\
                VencChn, s32Ret);
        return s32Ret;
    }

    /******************************************
     step 3:  Regist Venc Channel to VencGrp
    ******************************************/
    s32Ret = HI_MPI_VENC_RegisterChn(VencGrp, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_RegisterChn faild with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /******************************************
     step 4:  Start Recv Venc Pictures
    ******************************************/
    s32Ret = HI_MPI_VENC_StartRecvPic(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}



HI_S32 SAMPLE_StartVenc(HI_U32 u32GrpCnt, HI_BOOL bHaveMinor,
    PAYLOAD_TYPE_E aenType[2], PIC_SIZE_E aenSize[2])
{
    HI_S32 i;
    HI_S32 s32Ret;
    VENC_GRP VeGroup;
    VENC_CHN VeChn;

    for (i=0; i<u32GrpCnt; i++)
    {
        VeGroup = i;
        VeChn = i;

        s32Ret = SAMPLE_COMM_VENC_Start(VeGroup, VeChn, aenType[0], gs_enViNorm,aenSize[0], SAMPLE_RC_CBR);
        if (s32Ret != HI_SUCCESS)
        {
            printf("\nSAMPLE_COMM_VENC_Start(%d,%d) err 0x%x\n", VeGroup, VeChn, s32Ret);
            return HI_FAILURE;
        }
    }

    return 0;
}
HI_S32 SAMPLE_StopVenc(HI_U32 u32GrpCnt, HI_BOOL bHaveMinor)
{
    HI_S32 s32Ret, i;
    VENC_GRP VeGroup;
    VENC_CHN VeChn;

    for (i=0; i<u32GrpCnt; i++)
    {
        VeGroup = i;
        VeChn = i;

        s32Ret = HI_MPI_VENC_StopRecvPic(VeChn);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_VENC_StopRecvPic vechn %d err 0x%x\n", VeChn, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VENC_UnRegisterChn(VeChn);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_VENC_UnRegisterChn vechn %d err 0x%x\n", VeChn, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VENC_DestroyChn(VeChn);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_VENC_DestroyChn vechn %d err 0x%x\n", VeChn, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VENC_DestroyGroup(VeGroup);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_VENC_DestroyGroup grp %d err 0x%x\n", VeGroup, s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_CreateJpegChn(VENC_GRP VeGroup, VENC_CHN SnapChn, PIC_SIZE_E enPicSize)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencAttr;
    VENC_ATTR_JPEG_S stJpegeAttr;

    //stVencAttr.enType = PT_JPEG;          //add by Mike
    //stVencAttr.pValue = &stJpegeAttr;

    SAMPLE_GetJpegeCfg(enPicSize, &stJpegeAttr);

    s32Ret = HI_MPI_VENC_CreateGroup(VeGroup);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateGroup err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_CreateChn(SnapChn, &stVencAttr);   //add by Mike
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateChn err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }

    /* This is recommended if you snap one picture each time. */
    s32Ret = HI_MPI_VENC_SetMaxStreamCnt(SnapChn, 1);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_SetMaxStreamCnt(%d) err 0x%x\n", SnapChn, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 Payload2FilePostfix(PAYLOAD_TYPE_E enPayload, HI_U8* szFilePostfix)
{
    if (PT_H264 == enPayload)
    {
        strcpy((char*)szFilePostfix, ".h264");
    }
    else if (PT_JPEG == enPayload)
    {
        strcpy((char*)szFilePostfix, ".jpg");
    }
    else if (PT_MJPEG == enPayload)
    {
        strcpy((char*)szFilePostfix, ".mjp");
    }
    else
    {
        printf("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/* save jpeg stream */
HI_S32 SampleSaveJpegStream(FILE* fpJpegFile, VENC_STREAM_S *pstStream)
{
    VENC_PACK_S*  pstData;
    HI_U32 i;

    fwrite(g_SOI, 1, sizeof(g_SOI), fpJpegFile);

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        pstData = &pstStream->pstPack[i];
        fwrite(pstData->pu8Addr[0], pstData->u32Len[0], 1, fpJpegFile);
        fwrite(pstData->pu8Addr[1], pstData->u32Len[1], 1, fpJpegFile);
    }

    fwrite(g_EOI, 1, sizeof(g_EOI), fpJpegFile);

    return HI_SUCCESS;
}

/* save h264 stream */
HI_S32 SampleSaveH264Stream(FILE* fpH264File, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        fwrite(pstStream->pstPack[i].pu8Addr[0],
               pstStream->pstPack[i].u32Len[0], 1, fpH264File);

        fflush(fpH264File);

        if (pstStream->pstPack[i].u32Len[1] > 0)
        {
            fwrite(pstStream->pstPack[i].pu8Addr[1],
                   pstStream->pstPack[i].u32Len[1], 1, fpH264File);

            fflush(fpH264File);
        }
    }

    return HI_SUCCESS;
}

HI_S32 SampleSaveVencStream(PAYLOAD_TYPE_E enType,FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 s32Ret;

    if (PT_H264 == enType)
    {
        s32Ret = SampleSaveH264Stream(pFd, pstStream);
    }
    else if (PT_MJPEG == enType)
    {
        s32Ret = SampleSaveJpegStream(pFd, pstStream);
    }
    else
    {
        return HI_FAILURE;
    }
    return s32Ret;
}

/* get stream from each channels and save them..
 * only for H264/MJPEG, not for JPEG.  */
HI_VOID* SampleGetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    PAYLOAD_TYPE_E enPayload;
    VENC_CHN VeChnStart;
    GET_STREAM_S* pstGet;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    HI_U8 szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32ret;

    pstGet = (GET_STREAM_S*)p;
    VeChnStart = pstGet->VeChnStart;
    s32ChnTotal = pstGet->s32ChnTotal;
    enPayload = pstGet->enPayload;

    printf("VeChnStart:%d, s32ChnTotal:%d\n",VeChnStart,s32ChnTotal);

    /* Prepare for all channel. */
    Payload2FilePostfix(enPayload, szFilePostfix);

    for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        sprintf(aszFileName[i], "stream_%s%d%s", "chn", i, szFilePostfix);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i])
        {
            printf("open file err!\n");
            return NULL;
        }
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] <= 0)
        {
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /* Start to get streams of each channel. */
    while (HI_TRUE == pstGet->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32ret < 0)
        {
            printf("select err\n");
            break;
        }
        else if (s32ret == 0)
        {
            printf("get venc stream time out, exit thread\n");
            break;
        }
        else
        {
            for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /* step 1: query how many packs in one-frame stream. */
                    memset(&stStream, 0, sizeof(stStream));
                    s32ret = HI_MPI_VENC_Query(i, &stStat);
                    if (s32ret != HI_SUCCESS)
                    {
                        printf("HI_MPI_VENC_Query:0x%x, chn:%d\n", s32ret, i);
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 2: malloc corresponding number of pack nodes. */
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        printf("malloc stream pack err!\n");
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 3: call mpi to get one-frame stream. */
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32ret = HI_MPI_VENC_GetStream(i, &stStream, HI_IO_BLOCK);
                    if (s32ret != HI_SUCCESS)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        printf("HI_MPI_VENC_GetStream err 0x%x\n", s32ret);
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 4: save stream. */
                    SampleSaveVencStream(pstGet->enPayload, pFile[i], &stStream);

                    /* step 5: release these stream */
                    s32ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (s32ret != HI_SUCCESS)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 6: free pack nodes */
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    };

    for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
    {
        fclose(pFile[i]);
    }
    pstGet->bThreadStart = HI_FALSE;
    return NULL;
}


static GET_STREAM_S s_stGetVeStream;

HI_S32 SAMPLE_StartVencGetStream(GET_STREAM_S *pstGetVeStream)
{
    memcpy(&s_stGetVeStream, pstGetVeStream, sizeof(GET_STREAM_S));

    s_stGetVeStream.bThreadStart = HI_TRUE;
    return pthread_create(&s_stGetVeStream.pid, 0, SampleGetVencStreamProc, (HI_VOID*)&s_stGetVeStream);
}

HI_S32 SAMPLE_StopVencGetStream()
{
    if (HI_TRUE == s_stGetVeStream.bThreadStart)
    {
        s_stGetVeStream.bThreadStart = HI_FALSE;
        pthread_join(s_stGetVeStream.pid, 0);
    }
    return HI_SUCCESS;
}

HI_VOID HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_ExitMPP();
        printf("MPP exit\n");
    }
    exit(0);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

