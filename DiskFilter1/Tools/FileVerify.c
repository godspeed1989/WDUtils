#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "md5.h"

typedef unsigned char u8;
#define MAXFILE		(600)
#define MAXSIZE		(1<<20)
#define PATHSTR		":\\Verify\\"

void Generate(char volume);
void Verify(const char* verify);
void do_md5_digest(const u8 *buf, const int len, u8 digest[16]);

int main(int argc, char *argv[])
{
	printf("g volume: generate random file\n");
	printf("v dt.txt: verify file digest\n");
	if (argc < 3)
	{
		return 1;
	}
	if (argv[1][0] == 'g')
	{
		Generate(argv[2][0]);
	}
	else if (argv[1][0] == 'v')
	{
		Verify(argv[2]);
	}
	else
	{
		return 1;
	}
	return 0;
}

void Generate(char volume)
{
	FILE *pfile, *pdigest;
	char name[512];
	u8 digest[16];
	u8 *file_content;
	unsigned int i, j, filesize;

	srand((unsigned int)time(NULL));
	printf("Generate %d files at [%c%s]...\n", MAXFILE, volume, PATHSTR);
	pdigest = fopen("dt.txt", "w");
	assert(pdigest);
	file_content = (u8*)malloc(MAXSIZE);
	assert(file_content);
	for (i = 0; i < MAXFILE; i++)
	{
		// create file
		sprintf(name, "%c%s%08d.dat", volume, PATHSTR, i);
		pfile = fopen(name, "wb");
		if (pfile == NULL)  break;
		// generate file content and close
		filesize = (rand()*rand()) % MAXSIZE;
		for (j = 0; j + sizeof(int) <= filesize; j+=sizeof(int))
			*((int*)(file_content + j)) = rand()*rand();
		// random write
		for (j = 0; j < filesize/2; j++)
			*(file_content + (rand()*rand()%filesize)) = (u8)rand();
		fwrite(file_content, 1, filesize, pfile);
		fclose(pfile);
		// calculate md5 and save
		do_md5_digest(file_content, filesize, digest);
		fprintf(pdigest, "%s\t", name);
		for(j = 0; j<16; j++)
			fprintf(pdigest, "%02x", digest[j]);
		fprintf(pdigest, "\n");
	}
	printf("Generate %d file(s).\n", i);
	fclose(pdigest);
	free(file_content);
}

void Verify(const char* verify)
{
	char name[512], str[8];
	u8 digest0[32], digest1[16];;
	FILE *pfile, *pdigest;
	u8 *file_content;
	unsigned int i, j, filesize, nerr;

	pdigest = fopen(verify, "r");
	assert(pdigest);
	file_content = (u8*)malloc(MAXSIZE);
	assert(file_content);
	printf("Start to verify file list [%s]...\n", verify);
	for (i = 0, nerr = 0; !feof(pdigest) && i < MAXFILE; i++)
	{
		fscanf(pdigest, "%s%s", name, digest0);
		// open and read file content
		pfile = fopen(name, "rb");
		if (pfile == NULL)  break;
		fseek(pfile, 0, SEEK_END);
		filesize = ftell(pfile);
		fseek(pfile, 0, SEEK_SET);
		fread(file_content, 1, filesize, pfile);
		// calculate md5 and compare
		do_md5_digest(file_content, filesize, digest1);
		for (j = 0; j<16; j++)
		{
			sprintf(str, "%02x", digest1[j]);
			if (str[0] == digest0[j*2] && str[1] == digest0[j*2+1]);
			else
			{
				nerr++;
				printf("%s check error\n", name);
			}
		}
		fclose(pfile);
	}
	printf("Verified %d file(s), %d error.\n", i, nerr);
	fclose(pdigest);
	free(file_content);
}

void do_md5_digest(const u8 *buf, const int len, u8 digest[16])
{
	md5_state_t state;
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)buf, len);
	md5_finish(&state, digest);
}
