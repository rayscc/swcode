/* swcode.c -v0.54 author:urays
 * data:2020-08-09 email:urays@foxmail.com */
#include <stdlib.h>
#include <string.h>

#include "swcode.h"

#define MALLOC(x,type,size)    do {while(!((x)=(type*)malloc(sizeof(type)*(size))));}while(0)
#define MEMOFREE(x)            do {free(x); x = NULL;}while(0)
#define MEMCPY(y,x,type,size)  do {memcpy((y),(x),sizeof(type)*(size_t)(size));}while(0)

#define TOPV             (255)
 /* 压缩帧 - 起始标志位[1] + 数据长度[2] + 压缩数据[...]*/
#define START_1          (TOPV) //起始标志 以1开始
#define START_0          (TOPV - 1) //起始标志 以0开始
#define START_2          (TOPV - 2) //压缩标志
#define PS_TWICE         (TOPV - 3) //二次压缩标志
#define DM               (TOPV - 4) //数据最大值

#define CACHE_SIZE       (16)
struct __cache {
	char* fn; struct __cache* nxt;  //文件名;指向下一个缓存
	int f, w, h; unsigned char** dt; //_总帧数_帧宽_帧高_数据集
}*SWCACHE[CACHE_SIZE] = { NULL }; //静态数据缓存

/* hash - times33*/
#define SWCF_HASH_FUNC(faddr)  do{                              \
    register unsigned int hash = 0;                             \
    const char* p = faddr;                                      \
    while (*p) { hash += (hash << 5) + *p++; }                  \
    hsi = hash & (CACHE_SIZE - 1);                              \
}while(0)

#define DIV_ROUND_UP(x,y)      (((x) + ((y) - 1)) / (y))
#define MR(d,x)                (((x) > 0)?(d >> x):(d << (-(x))))

/* 前位忽略重组为满字节流(X-BIN -> 8-BIN)  XXX1,1000 XXX0,1110 -> 1100,0011 1000,0000
 * @SBs:usrc首字节有效位起始位置(一般设为IGs)
 * @IGs:压缩时一字节前部忽略位数; IGs取值范围:[0,7]
 * @psz:输出结果字节数组大小 */
void X28BIN(unsigned char* usrc, int SBs, int IGs, unsigned char* pdst, int psz)
{
	const unsigned char M = 0xff >> IGs;
	const int VB = 8 - IGs;

	int R, L, i, j;
	unsigned char s = 0;
	unsigned char* tp = pdst;

	for (R = -SBs, L = 0, i = 0, j = 0; j < psz;)
	{
		if (L > 0)  //L >= 0 恒成立
		{
			s += ((*(usrc + i++) & M) << L);
			R = VB - L;
		}

		s += MR((*(usrc + i) & M), R);
		while (R < 0)
		{
			R += VB;
			s += MR((*(usrc + ++i) & M), R);
		}
		*(tp + j++) = s; s = 0;

		if (R > 0) { L = 8 - R; }
		else { R = -IGs; L = 0; ++i; }
	}
}

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

#define GBIT(ch,i)  (((ch) & ((0x01) << (7 - i))) >> (7 - i)) //获取字节上的任一位

int swcc_encode(unsigned char* usrc, int usize, unsigned char* pdst, int IGs)
{
	unsigned char chk = 0;     //非压缩数据第一位数据值
	unsigned char* otp = pdst; //压缩数据输出 操作指针
	unsigned char* s2t = pdst; //二次压缩起始指针 条件指针(是否待二次压缩数据长度大于8bit)
	unsigned char* t2p = NULL; //二次压缩操作指针 写入二次压缩数据
	int cnn = 0, n = 0, i = 0, j = 0, jn = 0;

	int b8sz = (int)(usize * (8 - IGs) / 8); //8-BIN时的真实字节数
	int SBs = IGs;  //usrc首字节有效位起始位置(一般设为IGs)

	//!
	for (i = 0, j = SBs; i < usize;) //预先评估压缩性能,若无效则不进行压缩
	{
		if (chk != GBIT(*(usrc + i), j)) {
			++jn;
			if (jn >= b8sz) { //处理不需要编码为swcode的帧
				cnn = b8sz + 3;
				*pdst = START_2;
				*(pdst + 1) = (unsigned char)(cnn & 255);
				*(pdst + 2) = (unsigned char)(cnn >> 8);
				X28BIN(usrc, SBs, IGs, pdst + 3, b8sz); //重组为8-BIN的满有效位格式
				return cnn;
			}
			chk = !chk;
		}
		if (++j >= 8) { j = IGs; ++i; }
	}
	//!

	//SWCODE
	chk = GBIT(*usrc, SBs);
	*otp++ = START_0 + chk; //压缩数据首
	*otp++ = 0, * otp++ = 0; //预留数据长度空间

	for (i = 0, j = SBs; i < usize;) { //IGNORE_BIT 跳过一个字节的前几位
		if (chk == GBIT(*(usrc + i), j)) { ++n; }
		else {
			MAKE_DATA(n);
			chk = !chk; //比较值 切换
			n = 1;
		}
		if (++j >= 8) { j = IGs; ++i; } //O(N)
	}
	if (n > 0) MAKE_DATA(n);

	cnn = (int)(otp - pdst);                  //压缩后的数据长度,也包括头信息
	*(pdst + 1) = (unsigned char)(cnn & 255);
	*(pdst + 2) = (unsigned char)(cnn >> 8);
	return cnn;
}

#define PAGESIZE(d1,d2)  ((int)d1 + ((int)d2 << 8)) //max: 65535

/* 获取一帧swcode所包含的字节数 */
static int swcc_code_size(unsigned char* psrc)
{
	return (psrc != NULL) ? ((*psrc == START_0 || *psrc == START_1 || *psrc == START_2) ?
		PAGESIZE(*(psrc + 1), *(psrc + 2)) : 0) : 0; //两位计算得数据长度
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
	unsigned char* tp = psrc, * up = udst;
	unsigned int rm = 0, tj = 0;

	if (psrc == NULL) { return 0; }
	chk = *tp++ - START_0; //获取非压缩数据第一位值
	rm = *tp++; rm += *tp++ << 8; //计算压缩数据长

	//!
	if (*psrc == START_2) { //不用担心读取长度溢出,在数据存入内存时已经做了处理
		MEMCPY(udst, psrc + 3, unsigned char, rm - 3); //可以直接读取的满8位数据格式
		return (rm - 3);
	}
	//!

	*up = 0; //初始化第一个输出字节 这个很关键
	for (buf = *tp++; tp - psrc <= rm; buf = *tp++)
	{
		switch (buf) {
		case(0): { //数据溢出标记
			MAKE_BYTE(DM);  //编写字节  DM 数据最大值
			for (buf = *tp++; buf != 0; buf = *tp++)
				MAKE_BYTE(buf);
		}break;
		case(PS_TWICE): {  //二次压缩标记
			buf = *tp++;   //二次压缩数据组组数
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

#include <stdio.h>  //提供文件操作

/* 默认参数说明:  fp - 文件句柄(FILE*)
				   m - 文件打开方式(目前只用了两种:wb/r+b)
			   fname - 文件名(char[])
				 buf - 读入数据缓存
				   s - 预读入buf的数据量
				  rc - fread()的返回值
				 o&x - 文件指针偏移  */

#define FOPEN(m)       {fp=fopen(fname, (m));}
#define FCLOSE         {fclose(fp); fp = NULL;}
#define FRD(buf,s,rc)  (rc=fread(buf, sizeof(unsigned char), s, fp))
#define FWR(buf,s)     (fwrite(buf, sizeof(unsigned char), s, fp))
#define FSK_ST(o)      (fseek(fp, o, SEEK_SET))
#define FSK_ED         (fseek(fp, 0, SEEK_END))
#define FSK_BK(x)      (fseek(fp, x, SEEK_CUR))  //回退x个单位
#define FSIZE()        {FSK_ED; fsize = ftell(fp); FSK_ST(0);}

/* 压缩文本 - SWC标记[3] +  数据值上限[1] + 包含帧数[2] + 每帧宽[1] + 每帧高[1] */
int swcf_push_code(const char* fname, unsigned char* psrc, int setw, int seth)
{
	int csize = 0, cnt = 0, rcnt = 0;
	unsigned char hd[8] = { '\0' }; //文件头信息临时存储数组
	FILE* fp = NULL;

	FOPEN("r+b"); //以二进制方式打开一个文本 只允许读写数据
	if (fp == NULL) { //如果没有则创建并初始化一个SWCODE
		FOPEN("wb");
		hd[0] = 's', hd[1] = 'w', hd[2] = 'c';
		hd[3] = 0, hd[4] = 0;        //存储帧数记录
		hd[5] = (unsigned char)setw; //记录每一帧宽度
		hd[6] = (unsigned char)seth; //记录每一帧高度
		FWR(hd, 7); FCLOSE;
		return swcf_push_code(fname, psrc, setw, seth);
	}
	else {
		csize = swcc_code_size(psrc); //计算压缩帧长度
		FSK_ST(3); FRD(hd + 3, 4, rcnt);
		cnt = PAGESIZE(hd[3], hd[4]);  //计算文件当前包含的帧数
		if (csize > 0 && hd[5] == setw && hd[6] == seth) { //检测目标PUSH文件的有效性
			if (hd[3] + 1 >= 256) { //新增记数
				hd[3] = 0;
				hd[4] += 1;
			}
			else { hd[3] += 1; }
			FSK_ST(3); FWR(hd + 3, 2);//重写文件信息头信息
			FSK_ED; FWR(psrc, csize);
			++cnt;
		}
	}
	FCLOSE;
	return cnt; //返回当前文件包含帧数
}

/* 回滚文件读取操作*/
#define ROLLBACK()   do{MEMOFREE(*((*_swcf)->dt + fn));--fn;}while(0)

/* 将包含多帧swcode格式文件写入内存中 */
static void move_to_memory(const char* fname, struct __cache** _swcf)
{
	int fn = -1, i = -1, rcnt = 0;
	int fsz = 0, fsz1 = 0, MFSZ = 0;
	unsigned char hd[8] = { '\0' }; //文件头信息临时存储数组
	unsigned char nn[4] = { '\0' };
	FILE* fp = NULL;

	FOPEN("rb"); //以二进制方式打开 只读
	if (fp != NULL) {
		FRD(hd, 7, rcnt);   //解析为有效的SWC文件
		if (hd[0] == 's' && hd[1] == 'w' && hd[2] == 'c')
		{
			MALLOC(*_swcf, struct __cache, 1);
			MALLOC((*_swcf)->fn, char, strlen(fname) + 1);
			strcpy((*_swcf)->fn, fname); //存储文件名
			(*_swcf)->f = PAGESIZE(hd[3], hd[4]); //存储帧数
			(*_swcf)->w = hd[5];         //数据宽度
			(*_swcf)->h = hd[6];         //数据高度
			MALLOC((*_swcf)->dt, unsigned char*, (*_swcf)->f); //为每一帧分配内存空间
			(*_swcf)->nxt = NULL;
			//允许帧的最大尺寸
			MFSZ = (int)(((*_swcf)->w << 3) * (*_swcf)->h / 8) + 3;

			for (; FRD(nn, 1, rcnt);) {
				if (nn[0] == START_1 || nn[0] == START_0 || nn[0] == START_2)
				{
					if (i != fsz1 - 1) { fsz1 = 0; i = -1; ROLLBACK(); }

					FRD(nn + 1, 2, rcnt);
					fsz = PAGESIZE(nn[1], nn[2]);//获取压缩数据长度
					if (fsz > MFSZ) { FSK_BK(-2); continue; }

					MALLOC(*((*_swcf)->dt + ++fn), unsigned char, fsz); //分配一帧的内存空间

					if (nn[0] == START_2)
					{
						FSK_BK(-3);  //文件指针回退3个单位
						FRD((*((*_swcf)->dt + fn)), fsz, rcnt);
						if (rcnt < fsz) { ROLLBACK(); }//实际读取字节数小于期望值,则撤销该帧
					}
					else {
						*(*((*_swcf)->dt + fn) + (i = 0)) = nn[0];
						*(*((*_swcf)->dt + fn) + (i = 1)) = nn[1];  //存储一帧的头信息
						*(*((*_swcf)->dt + fn) + (i = 2)) = nn[2];
						fsz1 = fsz;
					}
				}
				else if (i < fsz1 - 1) {
					*(*((*_swcf)->dt + fn) + ++i) = nn[0];
				}
			}
			if (i != fsz1 - 1) { ROLLBACK(); }//检查最后一帧数据是否有效
			(*_swcf)->f = fn + 1;
		}
		FCLOSE;
	}
}

#define SAME_FILE(fn,fnn)  (strcmp(fnn, fn) == 0)

/* 给新加入的swcode格式文件分配内存地址,并将其写入内存 */
static struct __cache* swcf_to_memory(const char* _fn)
{
	struct __cache* p = NULL, * sp = NULL;
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

/* 释放指定swcode格式文件的内存占用,并返回新的存储地址 */
static struct __cache** swcf_re_memory(const char* _fn)
{
	struct __cache* p = NULL, * sp = NULL;
	unsigned int hsi = 0;

	swcf_read_free(_fn);  //尝试清除残留数据
	SWCF_HASH_FUNC(_fn);
	if (SWCACHE[hsi] == NULL) { return &SWCACHE[hsi]; }
	else {
		sp = p = SWCACHE[hsi];
		while (p != NULL) { sp = p, p = p->nxt; }
		return &(sp->nxt);
	}
}

void en_bin_as_swcf(const char* fname, int IGs, int setw, int seth)
{
	int tcnt = DIV_ROUND_UP(setw * seth, (8 - IGs));
	int rcnt = 0; //从fname中,真实读取的字节数
	unsigned char* tarr = NULL, * earr = NULL;
	struct __cache** r = NULL;
	int fsize = 0, frms = -1, usz, i;

	FILE* fp = NULL;

	if (setw <= 0 || seth <= 0) { return; };
	MALLOC(tarr, unsigned char, tcnt);
	MALLOC(earr, unsigned char, (size_t)tcnt + 3); //+3是为了应对无效压缩的情况(标记符+2字节数据长度)
	FOPEN("r+b");  //以二进制方式打开一个文本 只允许读写数据
	if (fp != NULL) {
		FSIZE();
		frms = fsize / tcnt;

		r = swcf_re_memory(fname); //将BIN一次全部写入内存
		MALLOC(*r, struct __cache, 1);
		MALLOC((*r)->fn, char, strlen(fname) + 1);
		strcpy((*r)->fn, fname);
		(*r)->f = frms;
		(*r)->w = setw >> 3;
		(*r)->h = seth;
		MALLOC((*r)->dt, unsigned char*, frms);
		(*r)->nxt = NULL;

		for (i = 0; i < frms; i++)
		{
			FRD(tarr, tcnt, rcnt);
			if (rcnt != tcnt) { (*r)->f = frms = i + 1; break; }

			usz = swcc_encode(tarr, rcnt, earr, IGs);
			MALLOC(*((*r)->dt + i), unsigned char, usz);
			MEMCPY(*((*r)->dt + i), earr, unsigned char, usz);
		}
		FCLOSE
	}
	MEMOFREE(tarr);
	MEMOFREE(earr);
}

int swcf_read_form(const char* fname, int* getw, int* geth)
{
	struct __cache* fme = NULL;

	*getw = *geth = 0;
	if ((fme = swcf_to_memory(fname)) != NULL) {
		*getw = fme->w;
		*geth = fme->h;
	}
	return (fme != NULL) ? fme->f : -1; //返回文件总帧数
}

int swcf_read_code(const char* fname, int ifme, unsigned char** pdst)
{
	struct __cache* fme = NULL;

	if (pdst != NULL) {
		fme = swcf_to_memory(fname); //尝试将静态数据搬运到内存中
		*pdst = (fme != NULL) ? ((ifme >= fme->f || ifme < 0)
			? NULL : *(fme->dt + ifme)) : NULL; //获取某一帧压缩数据地址
		return swcc_code_size(*pdst); //获取压缩数据长度
	}
	return -1;
}

/* 根据存储结构释放指定的内存占用区域 */
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