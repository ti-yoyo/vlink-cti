#!/bin/bash
################################################################################
# 功能：
#    统一清理日志；清理的对象是那些无法被应用程序自动清理的日志；
#
# 适用平台：NFS
# 运行方式：利用crontab，设置执行一次;
# 20 2 * * * /usr/local/bin/log_handle.sh
# Author: anjb@ti-net.com.cn
################################################################################

if [ -f  /etc/profile ]; then
    . /etc/profile
fi
if [ -f ~/.bash_profile ]; then
    . ~/.bash_profile
fi

LOG_FILE_PATH=/var/nfs/logs/  #该脚本产生的日志文件的位置

echo "*******************日志处理*******************"
cur_time=$(date +"%Y-%m-%d %H:%M:%S")
echo "日志开始处理时间:${cur_time}"

#排除掉/var/nfs/logs/jbe-1/sip 只保留/var/nfs/logs/jbe-1/sip/20150404下面的目录
find ${LOG_FILE_PATH} -type d -mtime +3 -print | grep "/sip/" | xargs rm -rf 

find ${LOG_FILE_PATH} -type f -mtime +3 -print | xargs rm -f 

echo "*******************日志处理*******************"
cur_time=$(date +"%Y-%m-%d %H:%M:%S")
echo "日志开始处理完成时间:${cur_time}"
