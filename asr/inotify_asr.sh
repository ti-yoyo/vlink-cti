#!/bin/sh
# file name: inotify_asr.sh
# nohup /usr/local/bin/inotify_asr.sh > /dev/null &
export LC_CTYPE="zh_CN.UTF-8"

SRC=/var/nfs/voices/ASR
LOG_PATH=/var/log/ccic/asr
ASR_ENGINE=3 # 1:iFly(科大讯飞)
             # 2:UniSOUND(云知声)
	     # 3:tinet
             # 4:tinet sbc

if [[ ! -d ${LOG_PATH} ]]; then
	mkdir -p ${LOG_PATH}
fi
/usr/local/bin/inotifywait -mrq --timefmt '%s' --format '%T %f' --exclude monitor -e create ${SRC} \
| while read line
do
	file=$(echo ${line} | awk '{print $2}')
	folder=$(date +%Y%m%d)
    if [[ ${ASR_ENGINE} -eq 1 ]]; then
	    /usr/local/bin/asr_ifly.sh ${folder} ${file} >> ${LOG_PATH}/ifly-$(date +%Y%m%d).log &
    fi
    if [[ ${ASR_ENGINE} -eq 2 ]]; then
        /usr/local/bin/asr_unisound.sh ${folder} ${file} >> ${LOG_PATH}/unisound-$(date +%Y%m%d).log &
    fi
    if [[ ${ASR_ENGINE} -eq 3 ]]; then
        /usr/local/bin/asr_tinet.sh ${folder} ${file} >> ${LOG_PATH}/tinet-$(date +%Y%m%d).log &
    fi
    if [[ ${ASR_ENGINE} -eq 4 ]]; then
        /usr/local/bin/asr_tinet_sbc.sh ${file} >> ${LOG_PATH}/tinet_sbc-$(date +%Y%m%d).log &
    fi
done

