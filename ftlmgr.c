#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
// 필요한 경우 헤더파일을 추가한다

FILE *flashfp;	// fdevicedriver.c에서 사용

int dd_read(int ppn,char *pagebuf);
int dd_write(int ppn,char *pagebuf);
int dd_erase(int pbn);
//
// 이 함수는 FTL의 역할 중 일부분을 수행하는데 물리적인 저장장치 flash memory에 Flash device driver를 이용하여 데이터를
// 읽고 쓰거나 블록을 소거하는 일을 한다 (동영상 강의를 참조).
// flash memory에 데이터를 읽고 쓰거나 소거하기 위해서 fdevicedriver.c에서 제공하는 인터페이스를
// 호출하면 된다. 이때 해당되는 인터페이스를 호출할 때 연산의 단위를 정확히 사용해야 한다.
// 읽기와 쓰기는 페이지 단위이며 소거는 블록 단위이다.
// 
int main(int argc, char *argv[])
{	
	char sectorbuf[SECTOR_SIZE];
	char sparebuf[SPARE_SIZE];
	char pagebuf[PAGE_SIZE];
	char *blockbuf;
	char freebuf[PAGE_SIZE];
	char *fname;
	char sector[SECTOR_SIZE];
	char spare[SPARE_SIZE];

	if(strcmp(argv[1],"c")==0)
	{
		fname=argv[2];
		blockbuf=(char*)malloc(BLOCK_SIZE);
		if((flashfp=fopen(fname,"w+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname);
			exit(1);
		}
		memset(blockbuf,(char)0xFF,BLOCK_SIZE);
		int num=atoi(argv[3]);
		for(int i=0;i<num;i++)
		{
			fwrite(blockbuf,BLOCK_SIZE,1,flashfp);
			fseek(flashfp,0,SEEK_CUR);
		}
		fclose(flashfp);
	}
	if(strcmp(argv[1],"w")==0)
	{
		int ppn=atoi(argv[3]);
		int ret;
		fname=argv[2];

		if((flashfp=fopen(fname,"r+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname);
			exit(1);
		}
		memset(freebuf,(char)0xFF,PAGE_SIZE);//모두 0xff가 들어가는 버퍼
		if((ret=dd_read(ppn,pagebuf))<0)
		{
			fprintf(stderr,"file read error\n");
			exit(1);
		}

		if(memcmp(pagebuf,freebuf,PAGE_SIZE)!=0)//이미 내용이 있는경우
		{//우회적으로 write
			fseek(flashfp,0,SEEK_END);
			int fsize=ftell(flashfp);
			int num=fsize/BLOCK_SIZE;//현재 block의 총개수

			int f_ppn=0;//프리 블락의 페이지
			int i,j;
			for(i=0;i<num;i++)//프리블락 찾기
			{
				int count=0;
				for(j=0;j<4;j++)
				{
					dd_read(f_ppn,pagebuf);
					if(memcmp(pagebuf,freebuf,PAGE_SIZE)==0){
						count++;
						continue;
					}
					f_ppn++;
				}
				if(count==4)
					break;
			}

			// i를 프리블락이라 생각함
			//ppn의 블락을 찾아가서 프리블락으로 복사
			int pbn=ppn/4;
			char *tmp;
			f_ppn=i*4;//프리블락의 페이지
			int o_ppn=pbn*4;//원래 블락의 페이지
			for(int k=0;k<4;k++)//프리블락에 복사
			{
				memset(tmp,0,PAGE_SIZE);
				if(o_ppn==ppn)
				{
					o_ppn++;
					continue;
				}
				dd_read(o_ppn,pagebuf);
				if(memcmp(pagebuf,freebuf,PAGE_SIZE)!=0)
				{
					dd_read(f_ppn,tmp);
					memcpy(tmp,pagebuf,PAGE_SIZE);
				}
				f_ppn++;
				o_ppn++;
			}

			dd_erase(pbn);//원래 블락 데이터 제거
			//다시 원래자리로 붙이기
			f_ppn=i*4;
			o_ppn=pbn*4;
			memset(tmp,0,PAGE_SIZE);
			memset(pagebuf,0,PAGE_SIZE);
			for(int k=0;k<4;k++)
			{
				dd_read(f_ppn,tmp);
				if(memcmp(pagebuf,freebuf,PAGE_SIZE)!=0)
				{
					dd_read(o_ppn,pagebuf);
					memcpy(pagebuf,tmp,PAGE_SIZE);
				}
				o_ppn++;
				f_ppn++;
			}
			dd_erase(i);//프리블락 삭제
		}
	//새로운 데이터 추가
		memset(sectorbuf,(char)0xFF,SECTOR_SIZE);
		memset(sparebuf,(char)0xFF,SPARE_SIZE);
		memcpy(sectorbuf,argv[4],strlen(argv[4]));
		memcpy(sparebuf,argv[5],strlen(argv[5]));
		memcpy(pagebuf,sectorbuf,SECTOR_SIZE);
		memcpy(pagebuf+SECTOR_SIZE,sparebuf,SPARE_SIZE);

		dd_write(ppn,pagebuf);
		fclose(flashfp);
	}
	if(strcmp(argv[1],"r")==0)
	{
		fname=argv[2];
		if((flashfp=fopen(fname,"r"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname);
			exit(1);
		}
		int ppn=atoi(argv[3]);
		int ret;
		fseek(flashfp,0,SEEK_END);
		long fsize=ftell(flashfp);
		int num=fsize/PAGE_SIZE;
		if(ppn>(num-1))
		{
			fprintf(stderr,"This page number doesn't exist.\n");
			exit(1);
		}
		if((ret=dd_read(ppn,pagebuf))<0)
		{
			fprintf(stderr,"flash memory read error.\n");
			exit(1);
		}
		char c=0xFF;

		memset(sector,0,SECTOR_SIZE);//문자출력버퍼 초기화
		memset(spare,0,SPARE_SIZE);//문자출력버퍼 초기화

		memset(sectorbuf,0,SECTOR_SIZE);
		memcpy(sectorbuf,pagebuf,SECTOR_SIZE);

		int j=0;
		for(int i=0;i<SECTOR_SIZE;i++)
		{
			if(sectorbuf[i]==c)
				break;
			else {
				sector[j]=sectorbuf[i];
				j++;
			}
		}

		memset(sparebuf,0,SPARE_SIZE);
		memcpy(sparebuf,pagebuf+SECTOR_SIZE,SPARE_SIZE);
		j=0;
		for(int i=0;i<SPARE_SIZE;i++)
		{
			if(sparebuf[i]==c)
				break;
			else
			{
				spare[j]=sparebuf[i];
				j++;
			}
		}
		if(strlen(sector)>0&&strlen(spare)>0)
		{
			printf("%s %s\n",sector,spare);
		}
		fclose(flashfp);
	}
	else if(strcmp(argv[1],"e")==0)
	{
		fname=argv[2];
		if((flashfp=fopen(fname,"r+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname);
			exit(1);
		}
		int pbn=atoi(argv[3]);
		int ret=0;
		if((ret=dd_erase(pbn))<0)
		{
			fprintf(stderr,"block erase error\n");
			exit(1);
		}
		fclose(flashfp);
	}

	return 0;
}
