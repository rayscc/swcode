#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swcode.h"

//.bin: 120 * 60; 900byte
#define SRCBIN  "..\\swcode\\test\\VIDEO-1-120-60.bin"
#define UCodeW     120
#define UCodeH     60
#define UCodeSize  (UCodeW*UCodeH/8)

static unsigned char arr1[UCodeSize * 8] = { 0 };
static unsigned char arr2[UCodeSize + 3] = { 0 };

//add suffix. the returned value as the name of path
char* addsuffix(char* name, char* sfx)
{
	char* res = NULL;
	while ((res = (char*)malloc(strlen(name) + strlen(sfx))) == NULL) {}
	sprintf(res, "%s%s", name, sfx);
	return res;
}

//acquire the size of a static file
int filesize(const char* fn)
{
	int sz = -1;
	FILE* fp = fopen(fn, "r+b");
	if (fp != NULL) {
		fseek(fp, 0L, SEEK_END);
		sz = ftell(fp);
		fclose(fp);
	}
	return sz;
}

#define DIV_ROUND_UP(x,y)      (((x)+((y)-1))/(y))

//.bin => .swc
void bin_to_swc(const char* BIN, const char* SWC, int IGs)
{
	int t1, pt1;
	FILE* fp = NULL;

	if ((fp = fopen(BIN, "r+b")) != NULL)
	{
		int szz = DIV_ROUND_UP(UCodeSize * 8, 8 - IGs);
		printf(" [T] %s => %s\n", BIN, SWC);
		while (!feof(fp))
		{
			//read one frame
			t1 = fread(arr1, sizeof(unsigned char), szz, fp);
			if (t1 != szz) break;
			//recode one frame
			pt1 = swcc_encode(arr1, szz, arr2, IGs);
			//push one swcode to the tail of .swc
			swcf_push_code(SWC, arr2, UCodeW / 8, UCodeH);
		}
		fclose(fp);
		printf(" [*] psize:%d rate:%.2lf%%\n\n", filesize(SWC), filesize(SWC) / (filesize(BIN) * 0.01));
	}
}

//* => .bin
void swc_to_bin(const char* SWC, const char* BIN)
{
	int rcw = -1, rch = -1, infs = 0, i;
	unsigned char* pdst = NULL;
	FILE* fp = NULL;

	infs = swcf_read_form(SWC, &rcw, &rch);
	printf(" [T] %s => %s\n", SWC, BIN);
	printf(" [*] fs:%d  fw:%d  fh:%d\n", infs, rcw * 8, rch);
	if (infs > 0 && (fp = fopen(BIN, "w+b")) != NULL)
	{
		for (i = 0; i < infs; i++)
		{
			//read one swcode frame
			swcf_read_code(SWC, i, &pdst);
			//decode the swcode frame
			swcc_decode(pdst, arr2);
			//push one non-compressed frame to .swc.bin
			fwrite(arr2, sizeof(unsigned char), rcw * rch, fp);
		}
		fclose(fp);
	}
}

//compare file1 with file2
void march_check(const char* file1, const char* file2)
{
	int s1, s2, j, flg = 1;
	FILE* fp1 = fopen(file1, "r+b");
	FILE* fp2 = fopen(file2, "r+b");

	if (fp1 != NULL && fp2 != NULL)
	{
		printf(" [M] %s <=> %s\n", file1, file2);
		fseek(fp1, 0L, SEEK_END);
		fseek(fp2, 0L, SEEK_END);
		s1 = ftell(fp1);
		s2 = ftell(fp2);
		if (s1 == s2)
		{
			fseek(fp1, 0L, SEEK_SET);
			fseek(fp2, 0L, SEEK_SET);
			for (; !feof(fp1) && !feof(fp2);)
			{
				s1 = fread(arr1, sizeof(unsigned char), UCodeSize, fp1);
				s2 = fread(arr2, sizeof(unsigned char), UCodeSize, fp2);
				if (s1 != s2) { flg = 0; break; }  //check read size
				for (j = 0; j < UCodeSize; j++)
				{
					//compare each character of a frame
					if (arr1[j] != arr2[j]) break;
				}
				if (j != UCodeSize) { flg = 0; break; }
			}
		}
		else { flg = 0; }
		if (flg == 0) { printf(" [*] failed [][][][][][][][][][][][][][][]\n\n"); }
		else { printf(" [*] ok\n\n"); }
		fclose(fp1);
		fclose(fp2);
	}
}

#define GBIT(ch,i)  (((ch) & ((0x01) << (7 - i))) >> (7 - i))

//创建前部忽略的bin文件(用于生成测试样本)
void make_ignore_bin(const char* file, const int ignore_bit)
{
	FILE* fp = fopen(SRCBIN, "r+b");
	FILE* fp2 = fopen(file, "w+b");
	int s1 = 0, i, j;

	int kk = 7 - ignore_bit;
	unsigned char* b = (unsigned char*)malloc(sizeof(unsigned char) * UCodeSize);
	if (fp != NULL)
	{
		for (; !feof(fp);)
		{
			s1 = fread(b, sizeof(unsigned char), UCodeSize, fp);
			if (s1 != UCodeSize) break;

			int k = kk;
			unsigned char aa = 0;
			for (i = 0; i < UCodeSize; i++)
			{
				for (j = 0; j < 8; j++)
				{
					aa += (((unsigned char)GBIT(*(b + i), j)) << k);
					k--;
					if (k == -1)
					{
						k = kk;
						fputc(aa, fp2);
						aa = 0;
					}
				}
			}
			if (k != kk) fputc(aa, fp2);
		}
		fclose(fp2);
		fclose(fp);
	}
	free(b);
	b = NULL;
}

int main(int argc, char* argv[])
{
	int i = 0;
	char a[] = ".0.bin";
	char b[] = ".0.swc";
	char c[] = ".0.swc.bin";
	char d[] = ".0.mem.bin";
	char* MUBIN[9];
	char* MUSWC[9];
	char* MUSWC2BIN[9];
	char* MUMEMBIN[9];

	//生成前部忽略重组字节流(MUTATION)
	for (i = 0; i < 8; i++)
	{
		a[1] = (char)(i + 48);
		MUBIN[i] = addsuffix(SRCBIN, a);
		remove(MUBIN[i]);
		make_ignore_bin(MUBIN[i], i);

		b[1] = (char)(i + 48);
		MUSWC[i] = addsuffix(SRCBIN, b);
		remove(MUSWC[i]);

		c[1] = (char)(i + 48);
		MUSWC2BIN[i] = addsuffix(SRCBIN, c);
		remove(MUSWC2BIN[i]);

		d[1] = (char)(i + 48);
		MUMEMBIN[i] = addsuffix(SRCBIN, d);
		remove(MUMEMBIN[i]);
	}

	//swcode压缩\解压测试
	for (i = 0; i < 8; i++)
	{
		bin_to_swc(MUBIN[i], MUSWC[i], i);  //BIN => SWC
		swc_to_bin(MUSWC[i], MUSWC2BIN[i]); //SWC => BIN
		march_check(SRCBIN, MUSWC2BIN[i]);  //compare SRCBIN with MUSWC2BIN[i]
	}

	//test en_read_as_swcf()
	for (i = 0; i < 8; i++)
	{
		en_bin_as_swcf(MUBIN[i], i, UCodeW, UCodeH);
		swc_to_bin(MUBIN[i], MUMEMBIN[i]);  //tack MUBIN[i] out from memory
		march_check(SRCBIN, MUMEMBIN[i]);
	}

	system("pause");
	return 0;
}