#!/bin/sh

# 执行语音识别，被调用 $1 folder20150601 $2为语音文件名
cd /usr/local/bin/
log_path=/var/log/ccic/speech_recognize/
log_file=$log_path/speech_recognize.log.$(date +%Y%m%d)
aws_s3_path=s3://tinet-vocp
asr_path=/var/local/voices/ASR
if [ ! -d $log_path ]; then
    mkdir -p $log_path
fi

folder=$1
file_name=$2
res=`/usr/local/bin/match_pcm $asr_path/$folder/$file_name`
echo ${res}
echo "match_pcm $asr_path/$folder/$file_name res:${res}" >>${log_file}
mp3_file_name=${file_name%.wav}.mp3
result=`/usr/local/bin/lame $asr_path/$folder/$file_name $asr_path/$folder/${mp3_file_name} 2>&1`
echo "lame $asr_path/$folder/$file_name $asr_path/$folder/${mp3_file_name} res:$?" >>${log_file}
if [ $? -eq 0 ]; then
    if test -f $asr_path/$folder/${mp3_file_name} ; then
        aws s3 cp $asr_path/$folder/${mp3_file_name} ${aws_s3_path}/record_asr/${folder}/${mp3_file_name}
        if [ $? -eq 0 ]; then
            rm -f $asr_path/$folder/${mp3_file_name}
        else
            echo "upload to s3 failed file= $asr_path/$folder/${mp3_file_name} ${aws_s3_path}/record_asr/${folder}/${mp3_file_name}" >>${log_file}
        fi
    else
        echo "file=${mp3_file_name} not exist" >>${log_file}
    fi
else
    echo "failed to convert file=$file_name to mp3" >>${log_file}
fi
rm -f $asr_path/$folder/$file_name
