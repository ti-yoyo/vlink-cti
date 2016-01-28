#!/bin/sh

SRC=/var/nfs/voices/ASR

file=$2
folder=$1
channel=${file%.wav} # $UUID.wav
file=$folder/$file
hangup_cause=0
echo file=${file}
if [[ ! -f ${SRC}/${file} ]]; then
    echo "${SRC}/${file} NOT exists!"
    exit 0
fi
cd /usr/local/bin/

#等待4秒再检查
sleep 4
loop_count=1
seg_count=$(echo ${file} | awk -F'-' '{print NF}')
unix_ts=$(echo ${file} | awk -F'-' '{print $((NF-1))}')
if [ "x${file}" != "x" ]; then
    file_size=$(ls -l ${SRC}/${file} | awk '{print $5}')
    start_time=$(date +%s)
    while [[ ${file_size} -lt 64000 ]]; do
        now_time=$(date +%s)
        noanswer_last=$((${now_time}-${start_time}))
	echo noanswer_last=${noanswer_last}
        if test ${noanswer_last} -gt 5; then
            exit 0
        fi
        file_size=$(ls -l ${SRC}/${file} | awk '{print $5}')
        sleep 0.5
    done
    asr_start_time=$(date +%Y%m%d%H%M%S)
    
    #开始循环 判断是否有文件
    retry=0
    while [[ ${retry} -lt 10 ]]; do
    	if [[ -f ${SRC}/${file} ]]; then
    		hangup_cause=0
    		echo "/usr/local/bin/match_pcm ${SRC}/${file}"
    		txt=$(/usr/local/bin/match_pcm ${SRC}/${file})
    		echo "return ${txt}"
    		if [ "${txt}" == "关机" ]; then
    			hangup_cause=714
    			break
    		fi
    		if [ "${txt}" == "停机" ]; then
    			hangup_cause=716
    			break
    	        fi
    		if [ "${txt}" == "空号" ]; then
    			hangup_cause=713
    			break
    	        fi
    		if [ "${txt}" == "拒接" ]; then
            		hangup_cause=712
    			break
    		fi
            if [ "${txt}" == "快嘟嘟" ]; then
                    hangup_cause=710
                    break
            fi
            if [ "${txt}" == "嘟嘟" ]; then
                    hangup_cause=0
            fi
            if [ "${txt}" == "占线" ]; then
                    hangup_cause=710
                    break
            fi
            if [ "${txt}" == "暂时无法接通" ]; then
                    hangup_cause=715
                    break
            fi
            if [ "${txt}" == "秘书台" ]; then
                    hangup_cause=715
                    break
            fi
            if [ "${txt}" == "用户线故障" ]; then
                    hangup_cause=715
                    break
            fi
            if [ "${txt}" == "呼叫受限" ]; then
                    hangup_cause=715
                    break
            fi
            if [ "${txt}" == "未知" ]; then
                    hangup_cause=0
    		fi
            if [ "${txt}" == "彩铃" ]; then
                    hangup_cause=0
                    break
            fi
    	else
        	echo "${SRC}/${file} has been removed!"
    	fi
	sleep 2;
	retry=$((${retry}+1))
    done
    #结束循环
    echo "hangup_cause $hangup_cause" 
    if [[ ${hangup_cause} -gt 0 ]]; then
        #/usr/local/freeswitch/bin/fs_cli -x "uuid_kill ${channel} ${hangup_cause}"
	echo "/usr/local/freeswitch/bin/fs_cli -x \"uuid_kill ${channel} ${hangup_cause}\""
	echo "start time:${asr_start_time} | hangup time:$(date +%Y%m%d%H%M%S) | channel:${channel} | hangup cause:${hangup_cause}"
    fi

fi

