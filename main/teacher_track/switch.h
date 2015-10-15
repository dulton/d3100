#pragma once

/** 用于通知录播机进行切换 */

enum MovieScene
{
	MS_None, 	// 
	MS_TC = 1,		// 教师近景.
	MS_TF,		// 教师全景.
	MS_SC,		// 学生特写.
	MS_VGA,		// vga.
	MS_SF,		// 学生全景.
	MS_BD,		// 板书.
};

void ms_switch_to(MovieScene ms);

