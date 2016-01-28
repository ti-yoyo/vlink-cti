#ifndef _MATCH_PCM_H
#define _MATCH_PCM_H

#ifdef __cplusplus 
extern "C"
{ 
#endif 
//#include"common_this.h"

struct record;
extern int lord_record_flag;
extern struct record Record;
int lord_record(char* songs_file_name,char* record_file_name);
void free_record();
int match_pcm(short* pcm,int pcm_len,int sample,char* result_name);

#ifdef __cplusplus 
}
#endif  

#endif
