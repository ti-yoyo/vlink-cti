#!/bin/bash
################################################################################
# 功能：
#     1.删除NFS服务器上备份的7天前的wav文件（备份目录在/var/nfs/vocp/voice/backup/），
# 运行平台：NFS
# 运行方式：利用crontab，设置执行一次;
# 00 2 * * * /usr/local/bin/remove_old_wav.sh
# Author: anjb@ti-net.com.cn
################################################################################

backup_wav_path="/var/nfs/vocp/voices/backup"


echo "*******************日志处理*******************"
cur_time=$(date +"%Y-%m-%d %H:%M:%S")
echo "日志开始处理时间:${cur_time}"
if [ ! -d ${log_file_path} ]; then
    mkdir -p ${log_file_path}
fi

# 删除7天前wav
if [ -d ${backup_wav_path} ]; then
    find $backup_wav_path -name "*.wav" -mtime +3 -print | xargs rm -f 
fi

echo "*******************日志处理*******************"
cur_time=$(date +"%Y-%m-%d %H:%M:%S")
echo "日志开始处理完成时间:${cur_time}"