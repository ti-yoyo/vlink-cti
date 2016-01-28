##########################################################
#
#从monitor转成mp3并上传，同时移动到backup目录
# 启动方式：
# nohup /usr/local/bin/move_to_voices.sh >/dev/null 2>&1 &
#########################################################

#!/bin/bash
sounds_path=/var/spool/asterisk/monitor
backup_path=/var/nfs/vocp/voices/backup
mp3_path=/var/local/voices/mp3
nfs_mp3_path=/var/nfs/vocp/voices/mp3
monitor_path=/var/local/voices/monitor
log_path=/var/log/ccic/move_to_voices
log_file=$log_path/move_to_voices.log.$(date +%Y%m%d)
aws_s3_path=s3://tinet-vocp


if [ ! -d $monitor_path ]; then
    mkdir -p $monitor_path
    chmod 775 $monitor_path
fi

if [ ! -d $log_path ]; then
    mkdir -p $log_path
fi
cd $monitor_path
move_to_voices() {
    for monitor_file in $(ls ${monitor_path} | head -n 100)
    do
        folder=`cat ${monitor_file}`
        file_name=${monitor_file}

        grep_record=`echo $file_name | grep "record"`

        if [ "x${grep_record}" != "x" ]; then
            echo "`date +%Y-%m-%d@%H:%M:%S` - ${file_name}" >>${log_file}
            result=`/usr/local/bin/lame ${sounds_path}/${file_name}.wav ${mp3_path}/${file_name}.mp3 2>&1`
            echo "lame result:$?" >>${log_file}
            if [ $? -eq 0 ]; then
                if test -f ${mp3_path}/${file_name}.mp3 ; then
                    aws s3 cp ${mp3_path}/${file_name}.mp3 ${aws_s3_path}/record/${folder}/${file_name}.mp3
               	    echo "s3 cp result:$?" >>${log_file}
                    if [ $? -eq 0 ]; then
                        rm -f ${mp3_path}/${file_name}.mp3
                    else
                        echo "upload to s3 failed file=${mp3_path}${file_name}.mp3" >>${log_file}	
                        mv ${mp3_path}/${file_name}.mp3 ${nfs_mp3_path}/${file_name}.mp3
                    fi
                    rm -f ${sounds_path}/${file_name}.wav
                else
                    mv ${sounds_path}/${file_name}.wav ${backup_path}/${file_name}.wav
                    echo "file=${mp3_path}/${file_name}.mp3 not exist" >>${log_file}
                fi
            else
                echo "failed converting ${sounds_path}/${file_name}.wav" >>${log_file}
                mv ${sounds_path}/${file_name}.wav ${backup_path}/${file_name}.wav
            fi
        fi

        grep_voicemail=`echo $file_name | grep "voicemail"`
        if [ "x${grep_voicemail}" != "x" ]; then
            echo "`date +%Y-%m-%d@%H:%M:%S` - ${file_name}" >>${log_file}
            result=`/usr/local/bin/lame ${sounds_path}/${file_name}.wav ${mp3_path}/${file_name}.mp3 2>&1`
            echo "lame result:$?" >>${log_file}
            if [ $? -eq 0 ]; then
                if test -f ${mp3_path}/${file_name}.mp3 ; then
                    aws s3 cp ${mp3_path}/${file_name}.mp3 ${aws_s3_path}/voicemail/${folder}/${file_name}.mp3
                    echo "s3 cp result:$?" >>${log_file}
        	    if [ $? -eq 0 ]; then
                        rm -f ${mp3_path}/${file_name}.mp3
                    else
                        echo "upload to s3 failed file=${mp3_path}${file_name}.mp3" >>${log_file}
                        mv ${mp3_path}/${file_name}.mp3 ${nfs_mp3_path}/${file_name}.mp3
                    fi
                    rm -f ${sounds_path}/${file_name}.wav
                else
                    echo "file=${mp3_path}/${file_name}.mp3 not exist" >>${log_file}
                    mv ${sounds_path}/${file_name}.wav ${backup_path}/${file_name}.wav
                fi
            else
                echo "failed converting ${sounds_path}/${file_name}.wav" >>${log_file}
                mv ${sounds_path}/${file_name}.wav ${backup_path}/${file_name}.wav
            fi
        fi
        rm -f ${monitor_file}
    done
}


while :
do
    move_to_voices >> ${log_path}/move_to_voices.log.$(date +%Y%m%d)
    sleep 1
done
