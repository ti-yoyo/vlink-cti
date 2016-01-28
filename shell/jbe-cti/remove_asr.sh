##########################################################
#
#从monitor转成mp3并上传，同时移动到backup目录
# 启动方式：
# nohup /usr/local/bin/remove_asr.sh >/dev/null 2>&1 &
#########################################################

#!/bin/bash

log_path=/var/log/ccic/remove_asr
log_file=$log_path/remove_asr.log.$(date +%Y%m%d)
BASE_URL=https://sqs.cn-north-1.amazonaws.com.cn/147022339119
systemName=`cat /var/run/systemName`

if [ ! -d $log_path ]; then
    mkdir -p $log_path
fi

remove_asr() {
    result=`aws sqs receive-message --queue-url ${BASE_URL}/VOCP-SQS-${systemName}`
    file_name=`echo "$result"|grep '"Body"'|awk -F '"' '{print $4}'`
    receipt_handle=`echo "$result"|grep '"ReceiptHandle"'|awk -F '"' '{print $4}'`
    if [ "$file_name" != "" ]; then
        if [ "$receipt_handle" != "" ]; then
	    aws sqs delete-message --queue-url ${BASE_URL}/VOCP-SQS-${systemName} --receipt-handle $receipt_handle
            echo "delete ASR filename: $file_name"
            rm -f $file_name
        else
           sleep 1
        fi
    else
        sleep 1
    fi
}

remove_asr_monitor() {
    monitor_path=/var/local/voices/remove_asr
    cd $monitor_path

    for monitor_file in $(ls ${monitor_path} | head -n 100)
    do
        folder=`cat ${monitor_file}`
        echo "delete ASR monitor_file: $monitor_file wav_file:/var/local/voices/ASR/${folder}/${monitor_file}-in.wav"
        rm -f ${monitor_file}
        rm -f /var/local/voices/ASR/${folder}/${monitor_file}-in.wav
    done    
    sleep 1
}

while :
do
    #remove_asr >> ${log_path}/move_to_voices.log.$(date +%Y%m%d)
    remove_asr_monitor >> ${log_path}/move_to_voices.log.$(date +%Y%m%d)
done
