/* swcode.c -v0.54 author:urays
 * data:2020-08-09 email:urays@foxmail.com */
#ifndef __SWC_CODE_H
#define __SWC_CODE_H

#define __swcc __declspec(dllexport)
 //#define __swcc extern

#ifdef __cplusplus
extern "C" {
#endif
	/*<!-- SWITCH CODE -->*/

	/* @概要 - 压缩一帧二进制数据
	 * @参数 - 待压缩数据首地址 | 待压缩数据字节总数 | 处理后数组首地址 | 压缩时一字节前部忽略位数
	 * @返回 - 处理后数据长度(int)
	 * @注意 - IGs取值范围:[0,7]; pdst对应的数组大小应该 >= usize+3 */
	__swcc int swcc_encode(unsigned char* usrc, int usize, unsigned char* pdst, int IGs);

	/* @概要 - 解压一帧数据
	 * @参数 - 待解压数据首地址 | 解压后的数据首地址
	 * @返回 - 解压后的数据长度(int) */
	__swcc int swcc_decode(unsigned char* psrc, unsigned char* udst);

	/*<!-- SWITCH CODE FILE -->*/ /* fopen fclose fseek fread fwrite */

	/* @概要 - 创建包含多帧的静态文本
	 * @参数 - 目标文本名 | 每帧数据首地址 | 每帧图像数据的宽度 | 每帧图像数据的高度
	 * @提示 - 120 * 60的二值化图像 则 setw = 120 / 8 = 15,seth = 60
	 * @返回 - 压入当前帧后目标文本包含的总帧数(int) */
	__swcc int swcf_push_code(const char* fname, unsigned char* psrc, int setw, int seth);

	/* @概要 - 使读BIN文件像读SWC文件一样(一次全部调入内存)
	 * @注意 - BIN文件读取格式通过 "IGs" 参数
	 * @参数 - 目标文本名 | 压缩时一字节前部忽略位数 | 设置每帧图像的宽度(像素) | 设置每帧图像的高度(像素)
	 * @注意 - IGs取值范围:[0,7]; 在"swcf_read_form"之前调用,调用后不能再进行"swcf_read_free"操作 */
	__swcc void en_bin_as_swcf(const char* fname, int IGs, int setw, int seth);

	/* @概要 - 读取文本数据格式
	 * @参数 - 目标文本名 | 包含的总帧数 | 每帧数据的宽度 | 每帧数据的高度
	 * @提示 - 获得的getw为一行字节数,其8倍为真正的图像宽
	 * @返回 - 文本包含的总帧数(int) [>0]:读取成功; [<=0]:文件读取失败 */
	__swcc int swcf_read_form(const char* fname, int* getw, int* geth);

	/* @概要 - 读取静态文本中指定帧(未解压)
	 * @参数 - 静态文本地址 | 第几帧 范围(0 ~ 总帧数 - 1) | 目标帧数据访问地址
	 * @返回 - 指定压缩帧数据长度 */
	__swcc int swcf_read_code(const char* fname, int iframe, unsigned char** pdst);

	/* @概要 - 释放指定文本缓存
	 * @参数 - 待释放内存对应的目标文本名 */
	__swcc void swcf_read_free(const char* fname);

#ifdef __cplusplus
}
#endif

#endif /*__SWC_CODE_H*/
