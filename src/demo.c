#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swcode.h"

//.bin: 120 * 60; 900byte
#define SRCBIN  "..\\swcode\\test\\test.bin"
#define UCodeW     120
#define UCodeH     60
#define UCodeSize  (UCodeW*UCodeH/8)

static unsigned char arr1[UCodeSize] = { 0 };
static unsigned char arr2[UCodeSize] = { 0 };

//add suffix. the returned value as the name of path
char* addsuffix(char* sfx) 
{
	char* res = NULL;
	while ((res = (char*)malloc(strlen(SRCBIN) + strlen(sfx))) == NULL) {}
	sprintf(res, "%s%s", SRCBIN, sfx);
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

//.bin => .swc
void bin_to_swc(const char* BIN, const char* SWC)
{
	int t1, pt1;
	FILE* fp = NULL;

	if ((fp = fopen(BIN, "r+b")) != NULL)
	{
		printf(" [T] %s => %s\n", BIN, SWC);
		while (!feof(fp))
		{
			//read one frame
			t1 = fread(arr1, sizeof(unsigned char), UCodeSize, fp);
			if (t1 < UCodeSize) break;
			//recode one frame
			pt1 = swcc_encode(arr1, UCodeSize, arr2);
			//push one swcode to the tail of .swc
			swcf_push_code(SWC, arr2, UCodeW / 8, UCodeH);
		}
		fclose(fp);
		printf(" [*] rate:%.2lf%%\n\n", filesize(SWC) / (filesize(BIN) * 0.01));
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
		if (flg == 0) { printf(" [*] failed\n\n"); }
		else { printf(" [*] ok\n\n"); }
		fclose(fp1);
		fclose(fp2);
	}
}

int main(int argc, char* argv[])
{
	char* NEWSWC = addsuffix(".swc");
	char* NEWBIN = addsuffix(".swc.bin");
	char* OUTBIN = addsuffix(".out");
	remove(NEWSWC), remove(NEWBIN), remove(OUTBIN);

	bin_to_swc(SRCBIN, NEWSWC); //SRCBIN => NEWSWC
	swc_to_bin(NEWSWC, NEWBIN); //NEWSWC => NEWBIN
	march_check(SRCBIN, NEWBIN);//compare SRCBIN with NEWBIN

	//test en_read_as_swcf()
	en_bin_as_swcf(SRCBIN, UCodeW, UCodeH);
	swc_to_bin(SRCBIN, OUTBIN);  //tack SRCBIN out from memory 
	march_check(SRCBIN, OUTBIN);

	system("pause");
	return 0;
}