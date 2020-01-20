#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swcode.h"

//.bin: 120 * 60; 900byte
#define SOURCEPATH  "..\\swcode\\test\\test.bin"
#define UCodeW     120
#define UCodeH     60
#define UCodeSize  (UCodeW*UCodeH/8)

//add suffix. the returned value as the name of path
char* addsuffix(char* sfx) 
{
	char* res = NULL;
	while ((res = (char*)malloc(strlen(SOURCEPATH) + strlen(sfx))) == NULL) {}
	sprintf(res, "%s%s", SOURCEPATH, sfx);
	return res;
}

//acquire the size of a static file
int filesize(char* filename)
{
	FILE* fp = fopen(filename, "r+b");
	int sz = -1;
	if (fp != NULL) {
		fseek(fp, 0L, SEEK_END);
		sz = ftell(fp);
	}
	return sz;
}

int main(int argc, char* argv[])
{
	char* NEWSWC = addsuffix(".swc");
	char* NEWBIN = addsuffix(".swc.bin");
	remove(NEWSWC),remove(NEWBIN);

	//.bin => .swc
	static unsigned char src[UCodeSize] = { 0 };
	static unsigned char psrc[UCodeSize] = { 0 };
	int t1, pt1, fcount = 0;
	FILE* fp = NULL;

	if ((fp = fopen(SOURCEPATH, "r+b")) != NULL)
	{
		printf("[recode] %s\n      => %s\n", SOURCEPATH, NEWSWC);
		while (!feof(fp))
		{
			//read one frame
			t1 = fread(src, sizeof(unsigned char), UCodeSize, fp);
			if (t1 < UCodeSize) break;
			//recode one frame
			pt1 = swcc_encode(src, UCodeSize, psrc);
			//push one swcode to the tail of .swc
			swcf_push_code(NEWSWC, psrc, UCodeW / 8, UCodeH);
		}
		fclose(fp);
		printf("shriking rate:%.2lf%%\n\n", filesize(NEWSWC) / (filesize(SOURCEPATH) * 0.01));
	}

	//check .swc, .swc => .bin
	int rcw = -1, rch = -1, infs = 0, i, j;
	unsigned char* pdst = NULL;
	static unsigned char udst[UCodeSize] = { 0 };

	if ((infs = swcf_read_form(NEWSWC, &rcw, &rch)) > 0)
	{
		printf("[read] from %s", NEWSWC);
		printf("\n    => fs:%d  fw:%d  fh:%d\n", infs, rcw * 8, rch);
		if ((fp = fopen(NEWBIN, "w+b")) != NULL)
		{
			for (i = 0; i < infs; i++)
			{
				//read one swcode frame
				swcf_read_code(NEWSWC, i, &pdst);
				//decode the swcode frame
				swcc_decode(pdst, udst);
				//push one non-compressed frame to .swc.bin
				fwrite(udst, sizeof(unsigned char), rcw * rch, fp);
			}
			fclose(fp);
		}
	}

	//compare .swc.bin with source file(.bin)
	int s1, s2;
	FILE* fp1 = fopen(SOURCEPATH, "r+b");
	FILE* fp2 = fopen(NEWBIN, "r+b");

	if (fp1 != NULL && fp2 != NULL)
	{
		printf("\n[match] %s\n     => %s\n", SOURCEPATH, NEWBIN);
		for (i = 0; i < infs; i++)
		{
			s1 = fread(src, sizeof(unsigned char), UCodeSize, fp1);
			s2 = fread(udst, sizeof(unsigned char), UCodeSize, fp2);
			if (s1 != s2) break;  //check read size
			for (j = 0; j < UCodeSize; j++)
			{
				//compare each character of a frame   
				if (src[j] != udst[j]) break;
			}
			if (j != UCodeSize) break;
		}
		if (i != infs) { printf("[%d] failed\n", i); }
		else { printf("     => ok\n"); }
		fclose(fp1), fclose(fp2);
	}

	system("pause");
	return 0;
}