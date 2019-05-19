/* swcode.c -v0.41 author:rayzco
 * data:2019-05-19 email:zhl.rays@outlook.com */
#ifndef __SWC_CODE_H
#define __SWC_CODE_H

#define __swcc __declspec(dllexport)
//#define __swcc extern

#define LIMIT_V     (255) /*压缩数据字节值大小范围控制 有效范围:5 ~ 255*/
#define IGNORE_BIT  (0)   /*压缩时一字节忽略位数前 max = 7 min = 0*/

/*<!-- SWITCH CODE -->*/

/* @概要 - 压缩一帧二进制数据
 * @参数 - 待压缩数据首地址 | 待压缩数据字节长 | 处理后数组首地址
 * @返回 - 处理后数据长度(int) */
__swcc int swcc_encode(unsigned char* usrc, int usize, unsigned char* pdst);

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

/* @概要 - 读取文本数据格式
 * @参数 - 目标文本名 | 包含的总帧数 | 每帧数据的宽度 | 每帧数据的高度
 * @提示 - 获得的getw为一行字节数,其8倍为真正的图像宽
 * @返回 - 文本包含的总帧数(int) [>0]:读取成功; [<=0]:文件读取失败 */
__swcc int swcf_read_form(const char* fname, int* getw, int* geth);

/* @概要 - 读取静态文本中指定帧
 * @参数 - 静态文本地址 | 第几帧 范围(0 ~ 总帧数 - 1) | 目标帧数据访问地址
 * @返回 - 指定压缩帧数据长度 */
__swcc int swcf_read_code(const char* fname, int iframe, unsigned char** pdst);

/* @概要 - 释放指定文本缓存
 * @参数 - 待释放内存对应的目标文本名 */
__swcc void swcf_read_free(const char* fname);


#endif /*__SWC_CODE_H*/
