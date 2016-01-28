##########################################################
#
#重新将mp3上传
# 启动方式：
# 定时任务
# 运行方式：利用crontab，设置执行一次;
# 0/10 * * * * /usr/local/bin/reupload_to_s3.sh
#########################################################

#!/bin/bash

mp3_path=/var/nfs/vocp/voices/mp3
log_path=/var/log/ccic/reupload_to_s3
aws_s3_path=s3://tinet-vocp

if [ ! -d $log_path ]; then
    mkdir -p $log_path
fi

log_file=${log_path}/reupload_to_s3.log.$(date +%Y%m%d)

for file in `ls ${mp3_path}|grep voicemail` ;do
    current_date=`echo ${file:8:8}`
    echo  "${file} ${current_date}"
    aws s3 cp ${mp3_path}/${file} ${aws_s3_path}/voicemail/${current_date}/${file}
    echo "s3 cp result:$?" >>${log_file}
    if [ $? -eq 0 ]; then
        echo "upload to s3 success ${aws_s3_path}/voicemail/${current_date}/${file}" >>${log_file}
        rm -f ${mp3_path}/${file}
    else
        echo "Failed upload ${mp3_path}/${file} to ${aws_s3_path}/voicemail/${current_date}/${file}">>${log_file}
    fi
done
for file in `ls ${mp3_path}|grep record` ;do
    current_date=`echo ${file:8:8}`
    echo  "${file} ${current_date}"
    aws s3 cp ${mp3_path}/${file} ${aws_s3_path}/record/${current_date}/${file}
    echo "s3 cp result:$?" >>${log_file}
    if [ $? -eq 0 ]; then
        echo "upload to s3 success ${aws_s3_path}/record/${current_date}/${file}">>${log_file}
        rm -f ${mp3_path}/${file}
    else
        echo "Failed upload ${mp3_path}/${file} to ${aws_s3_path}/voicemail/${current_date}/${file}">>${log_file}
    fi
done
