/* swcode.c -v0.32 author:rayscoo
 * data:2019-01-19 email:zhl.rays@outlook.com */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swcode.h"

#define FOREACH(m)          for(i = 0;i < m;++i)

#define MALLOC(type,size)   ((type*)malloc(sizeof(type)*size))
#define MEMOFREE(x)         do {free(x); x = NULL;}while(0)

 /* 压缩帧 - 起始标志位[1] + 数据长度[2] + 压缩数据[...] */

#define START_1      (RANGE_0T)     //起始标志 以1开始
#define START_0      (RANGE_0T - 1) //起始标志 以0开始
#define PS_TWICE     (RANGE_0T - 2) //二次压缩标志
#define DM           (RANGE_0T - 3) //数据最大值  

#define CACHE_SIZE   (16)
struct __cache {
	char* fn; struct __cache* nxt;   //文件名;指向下一个缓存
	int f, w, h; unsigned char** dt; //总帧数_帧宽_帧高_数据集
}*SWCACHE[CACHE_SIZE] = { NULL }; //静态数据缓存

/* hash - times33*/
#define SWCF_HASH_FUNC(faddr)  do{                              \
    register unsigned int hash = 0;                             \
    const char* p = faddr;                                      \
    while (*p) { hash += (hash << 5) + *p++; }                  \
    hsi = hash & (CACHE_SIZE - 1);                              \
}while(0) 

#define SAME_FILE(fn,fnn)  (strcmp(fnn, fn) == 0) 

/* @num:数据值 @otp:输出地址指针
 * @t2p:二次压缩活动指针 @s2t:二次扫描起始地址*/
#define MAKE_DATA(num)  do {                                    \
    if ((num) > DM) {                                           \
        *otp++ = 0; (num) -= DM;                                \
        for (; (num) > DM; (num) -= DM) { *otp++ = DM; }        \
        *otp++ = (unsigned char)(num); *otp++ = 0; }            \
    else {                                                      \
        *otp = (unsigned char)(num); t2p = otp++;               \
        if ((int)(otp - s2t) >= 8) {                            \
            if (*(t2p - 5) == PS_TWICE) {                       \
                if (*(t2p - 4) < DM && *t2p == *(t2p - 2)       \
                    && *(t2p - 1) == *(t2p - 3)) {              \
                    ++*(t2p - 4); otp = t2p - 1;                \
                }                                               \
                else { s2t = t2p - 5; }                         \
        }                                                       \
        else if (*(t2p - 4) != PS_TWICE) {                      \
            if (*t2p != 0 && *(t2p - 1) != 0) {                 \
                if (*t2p == *(t2p - 2) &&                       \
                    *(t2p - 1) == *(t2p - 3)) {                 \
                    *t2p = *(t2p - 2);                          \
                    *(t2p - 3) = PS_TWICE;                      \
                    *(t2p - 2) = 2; } } } } }                   \
}while(0)

static unsigned char bitc[8] = { 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01 };

#define GBIT(ch,i)  (((ch) & bitc[i]) >> (7 - i)) //获取字节上的任一位

int swcc_encode(unsigned char* usrc, int usize, unsigned char* pdst)
{
	unsigned char chk = 0;     //非压缩数据第一位数据值
	unsigned char* otp = pdst; //压缩数据输出 操作指针
	unsigned char* s2t = pdst; //二次压缩起始指针 条件指针(是否待二次压缩数据长度大于8bit)
	unsigned char* t2p = NULL; //二次压缩操作指针 写入二次压缩数据
	int cnn = 0, n = 0, i = 0, j = 0;

	chk = GBIT(*usrc, IGNORE_BIT);
	*otp++ = START_0 + chk; //压缩数据首
	*otp++ = 0, *otp++ = 0; //预留数据长度空间
	//IGNORE_BIT 跳过一个字节的前几位
	for (i = 0, j = IGNORE_BIT; i < usize;) {
		if (chk == GBIT(*(usrc + i), j)) { ++n; }
		else {
			MAKE_DATA(n);
			chk = !chk; //比较值 切换
			n = 1;
		}
		if (++j >= 8) { j = IGNORE_BIT; ++i; } //O(N)
	}
	if (n > 0) MAKE_DATA(n);

	cnn = (int)(otp - pdst); //压缩后的数据长度
	*(pdst + 1) = (unsigned char)(cnn & 255); //写入压缩数据长度信息
	*(pdst + 2) = (unsigned char)(cnn >> 8);
	return cnn;
}

static int swcc_code_size(unsigned char* psrc)
{
	return (psrc != NULL) ? ((*psrc == START_0 || *psrc == START_1) ?
		(int)(*(psrc + 1) + *(psrc + 2) * 256) : 0) : 0; //两位计算得数据长度
}

#define SBIT(ch,i)  (((ch) & 0x01) << (7 - i))    //向字节压入bit
#define SCHAR(ch)   (((ch) & 0x01) ? 0xff : 0x00) //直接输出一字节

/* @count:次数 @tj:8位限定标志
 * @chk:当前步类型[0/1]  @up:输出解压数据地址指针
 * @brief tj用于标记是否8位已满 未满一字节则先补充到8位 如果已补充为8位且n>8 则直接写入一字节 */
#define MAKE_BYTE(count)  do {                          \
    unsigned char n = count;                            \
    for (; 8 - tj > 0 && n > 0; n--) {                  \
        *up += SBIT(chk, tj);                           \
        ++tj;                                           \
    }                                                   \
    if (tj >= 8) { tj = *(++up) = 0; }                  \
    if (n > 0) {                                        \
        for (; n >= 8; n -= 8) {                        \
            *up++ = SCHAR(chk);                         \
        }                                               \
        for (*up = 0; n--;) {                           \
            *up += SBIT(chk, tj);                       \
            ++tj;  } }                                  \
}while(0)

int swcc_decode(unsigned char* psrc, unsigned char* udst)
{
	unsigned char buf = 0, chk = 0, f1 = 0, f2 = 0;
	unsigned char* tp = psrc, *up = udst;
	int rm = 0, tj = 0;

	if (psrc == NULL) { return 0; }
	chk = *tp++ - START_0; //获取非压缩数据第一位值
	rm = *tp++; rm += (*tp++) << 8; //计算压缩数据长
	*up = 0; //初始化第一个输出字节 这个很关键
	for (buf = *tp++; tp - psrc <= rm; buf = *tp++)
	{
		switch (buf) {
		case(0): { //数据溢出标记
			MAKE_BYTE(DM);  //编写字节  DM 数据最大值
			for (buf = *tp++; buf != 0; buf = *tp++)
				MAKE_BYTE(buf);
		}break;
		case(PS_TWICE): { //二次压缩标记
			buf = *tp++;  //二次压缩数据组组数
			f1 = *tp++, f2 = *tp++; //二次压缩数据组
			for (; buf--;) {
				MAKE_BYTE(f1); chk = !chk;
				MAKE_BYTE(f2); chk = !chk;
			}
			chk = !chk;
		}break;
		default: { MAKE_BYTE(buf); }
		}
		chk = !chk;
	}
	return (int)(up - udst); //输出解压后的数据
}

/* 压缩文本 - SWC标记[3] + 包含帧数[2] + 每帧宽[1] + 每帧高[1] + 压缩数据最大值[1] */
int swcf_push_code(const char* fname, unsigned char* psrc, int setw, int seth)
{
	int csize = 0, cnt = 0;
	unsigned char m = 0, n = 0, o = 0, p = 0;
	FILE* fp = fopen(fname, "r+b"); //以二进制方式打开一个文本 只允许读写数据

	if (fp == NULL) { //如果没有则创建并初始化一个SWCODE
		fp = fopen(fname, "wb");
		fprintf(fp, "swc%c%c%c%c%c", 0, 0, setw, seth, RANGE_0T);
		fclose(fp);
		return swcf_push_code(fname, psrc, setw, seth);
	}
	else {
		csize = swcc_code_size(psrc); //计算压缩帧长度
		fseek(fp, 3, SEEK_SET);
		fscanf(fp, "%c%c%c%c", &m, &n, &o, &p);
		cnt = m + n * 256; //计算文件当前包含的帧数
		if (csize > 0 && o == setw && seth == p) { //检测目标PUSH文件的有效性
			fseek(fp, 3, SEEK_SET);
			if (m + 1 >= 256) { //新增记数
				fputc(0, fp);
				fputc((unsigned char)(n + 1), fp);
			}
			else { fputc(m + 1, fp); }
			fseek(fp, 0, SEEK_END); //文件指针移动到文件尾
			fwrite(psrc, sizeof(unsigned char), csize, fp);
			++cnt;
		}
	}
	fclose(fp);
	return cnt; //返回当前文件包含帧数
}

#define HEADER_SWCF         (hd[0] == 's' && hd[1] == 'w' && hd[2] == 'c' && hd[7] == RANGE_0T)
#define HEADER_BASE(f,w,h)  do { f = hd[3] + hd[4] * 256; w = hd[5]; h = hd[6]; }while(0)

static void move_to_memory(const char* _fn, struct __cache** _swcf)
{
	unsigned char hd[9] = { '\0' }; //文件头信息临时存储数组
	unsigned char n = 0, n1 = 0, n2 = 0;
	unsigned short fn = -1, fsz = 0, i = 0;

	FILE* fp = fopen(_fn, "rb"); //以二进制方式打开 只读
	if (fp != NULL) {
		fread(hd, sizeof(unsigned char), 8, fp);
		if (HEADER_SWCF) //解析为有效的SWC文件
		{
			*_swcf = MALLOC(struct __cache, 1);
			(*_swcf)->fn = MALLOC(char, strlen(_fn) + 1);
			strcpy((*_swcf)->fn, _fn); //存储文件名

			HEADER_BASE((*_swcf)->f, (*_swcf)->w, (*_swcf)->h); //存储帧数_宽度_高度
			(*_swcf)->dt = MALLOC(unsigned char*, (*_swcf)->f); //为每一帧分配内存空间
			(*_swcf)->nxt = NULL;

			for (n = fgetc(fp); !feof(fp); n = fgetc(fp)) {
				if (n == START_0 || n == START_1) { //压缩数据的开始
					n1 = fgetc(fp);
					n2 = fgetc(fp);
					fsz = n1 + n2 * 256; //获取压缩数据长度
					*((*_swcf)->dt + ++fn) = MALLOC(unsigned char, fsz); //分配一帧的内存空间
					*(*((*_swcf)->dt + fn) + (i = 0)) = n;
					*(*((*_swcf)->dt + fn) + ++i) = n1;  //存储一帧的头信息
					*(*((*_swcf)->dt + fn) + ++i) = n2;
				}
				else if (i < fsz - 1) {
					*(*((*_swcf)->dt + fn) + ++i) = n;
				}
			}
		}
		fclose(fp);
	}
}

static struct __cache* swcf_to_memory(const char* _fn)
{
	struct __cache* p = NULL, *sp = NULL;
	unsigned int hsi = 0;

	SWCF_HASH_FUNC(_fn);  //计算获取哈希值
	if (SWCACHE[hsi] == NULL) { //内存中还没有
		move_to_memory(_fn, &SWCACHE[hsi]); //搬运静态存储到内存
		return SWCACHE[hsi]; //返回那片内存地址
	}
	else {
		sp = p = SWCACHE[hsi];
		while (p != NULL) { //链地址法
			sp = p;
			if (SAME_FILE(p->fn, _fn)) {
				return p;   //文件名比较 异同
			}               //此处的文件名为 文件完整的路径
			p = p->nxt;
		}
		move_to_memory(_fn, &sp->nxt);
		return sp->nxt;
	}
}

int swcf_read_form(const char* fname, int* getw, int* geth)
{
	unsigned char hd[9] = { '\0' };
	FILE* fp = fopen(fname, "rb");
	int fme = -1;

	*getw = *geth = 0;
	if (fp != NULL) {  //读取文件头信息
		fread(hd, sizeof(unsigned char), 8, fp);
		if (HEADER_SWCF) HEADER_BASE(fme, *getw, *geth);
		fclose(fp);
	}
	return fme; //返回文件总帧数
}

int swcf_read_code(const char* fname, int ifme, unsigned char** pdst)
{
	struct __cache* fme = swcf_to_memory(fname); //尝试将静态数据搬运到内存中

	*pdst = (fme != NULL) ? ((ifme >= fme->f || ifme < 0)
		? NULL : *(fme->dt + ifme)) : NULL; //获取某一帧压缩数据地址
	return swcc_code_size(*pdst); //获取压缩数据长度
}

static void free_one_cache(struct __cache** cur, struct __cache* nt)
{
	int fme = (*cur)->f;

	for (; fme--;) { //释放一个静态文件数据的内存空间
		MEMOFREE(*((*cur)->dt + fme)); //释放每一帧压缩数据
	}
	MEMOFREE((*cur)->dt);
	MEMOFREE((*cur)->fn);
	MEMOFREE((*cur));
	*cur = nt;
}

void swcf_read_free(const char* fname)
{
	unsigned int hsi = 0; //从缓存中释放指定文件的内存空间
	struct __cache* p = NULL;

	SWCF_HASH_FUNC(fname); //计算哈希值
	if ((p = SWCACHE[hsi]) != NULL) {
		if (SAME_FILE(SWCACHE[hsi]->fn, fname)) {
			free_one_cache(&SWCACHE[hsi], SWCACHE[hsi]->nxt);
		}
		else { //在哈希缓存中通过文件名寻找指定内存并释放
			while (p->nxt != NULL) {
				if (SAME_FILE(p->nxt->fn, fname)) {
					free_one_cache(&p->nxt, p->nxt->nxt);
					break;
				}
				p = p->nxt;
			}
		}
	}
}
