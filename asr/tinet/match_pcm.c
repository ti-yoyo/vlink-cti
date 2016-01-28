#include<stdlib.h>
#include<stdio.h>
#include <sys/stat.h>
#include"match_pcm.h"
#include"waveformat.h"

unsigned long get_file_size(const char* file_name){
	struct stat statbuff;
	if(stat(file_name, & statbuff)<0){
		return 0;
	}else{
		return statbuff.st_size;
	}
}
void main(int argc, char *argv[])
{
	int len=0;
	FILE* fp=NULL;
	char result_name[256];
	short *data=NULL;
	int size=0;
	waveFormat fmt;
	FILE *f=NULL;
	char* name=NULL;
	int sample=8000;
	if(argc != 2){
		printf("error: invalid param!\n");
		return;
	}
	if(lord_record("audio_file_name","record")){
		printf("error:lord_record() fail!\n");
		return;
	}
	name = argv[1];
	//printf("filename:%s\n",name);
	
	f=fopen(name,"rb");
	if(!f){
		printf("open the file %s fail!\n",name);
		free_record();
		return;
	}
        size = get_file_size(name);
        if(size < 64000){
                printf("size < 64KB!\n");
                return;
        }
	//printf("size=%d\n", size);
	size -= 44;
	data=(short *)malloc(sizeof(short)*size);
	fseek(f,44L,SEEK_SET);
	fread(data,sizeof(short),size,f);
	if(f){
		fclose(f);
		f=NULL;
	}

	if(match_pcm(data,size,sample,result_name)){
		printf("error:match_pcm fail!\n");
	}
	printf("%s\n",result_name);
	if(data){
		free(data);
		data=NULL;
	}
	free_record();
}
