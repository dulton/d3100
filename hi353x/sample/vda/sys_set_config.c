#ifndef __SYS_SET_CONFIG__
#define __SYS_SET_CONFIG__
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hi_type.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "mpi_sys.h"
#include "mpi_vb.h"

void bind()
{
    HI_S32 s32Ret;
    MPP_CHN_S stBindSrc;
    MPP_CHN_S stBindDest;
    stBindDest.enModId = HI_ID_VPSS;
    stBindDest.s32ChnId = 0;
    stBindDest.s32DevId = 0;

    stBindSrc.enModId = HI_ID_VIU;
    stBindSrc.s32ChnId = 0;
    stBindSrc.s32DevId = 0;

    s32Ret = HI_MPI_SYS_Bind(&stBindSrc, &stBindDest);
    if (s32Ret) {
	printf("binding failed!\n");
   	return HI_FAILURE; 
    }

    s32Ret = HI_MPI_SYS_UnBind(&stBindSrc, &stBindDest);
    if (s32Ret) {
    	printf("unbinding failed!\n");
	return HI_FAILURE;
    }

}
int main(void)
{
    HI_S32 s32ret;
    VB_CONF_S struVbConf;
    MPP_SYS_CONF_S strSysConf;

    memset(&struVbConf, 0, sizeof(VB_CONF_S));

    struVbConf.u32MaxPoolCnt = 64;
    struVbConf.astCommPool[0].u32BlkSize = 1920 * 1088 * 2;
    struVbConf.astCommPool[0].u32BlkCnt = 10;
    memset(struVbConf.astCommPool[0].acMmzName, 0, sizeof(struVbConf.astCommPool[0].acMmzName));
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    s32ret = HI_MPI_VB_SetConf(&struVbConf);
    if (HI_SUCCESS != s32ret) {
	printf("set vb setconfig failed!\n");
        return s32ret;
    }

    s32ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32ret) {
    	printf("mpp vb init failed!\n");	
    }

    strSysConf.u32AlignWidth = 16;
    s32ret = HI_MPI_SYS_SetConf(&strSysConf);
    if (HI_SUCCESS != s32ret) {
        printf("Set mpp  sys config failed!\n");
	return s32ret;
    }

    s32ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32ret) {
    	printf("Mpi init failed!\n");
	return s32ret;
    }
   
    bind();
    
    s32ret = HI_MPI_SYS_Exit();
    if (HI_SUCCESS != s32ret) {
    	printf("Mpi exit failed!\n");
	return s32ret;
    }
    s32ret = HI_MPI_VB_Exit();
    if (HI_SUCCESS != s32ret) {
	printf("mpi vb exit failed!\n");
    	return s32ret;
    }
    return 0;
}

#endif

