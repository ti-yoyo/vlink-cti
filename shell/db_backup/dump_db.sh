##########################################################
#
#备份CCIC2数据库
# 使用方式crontab 定时调用 一个库调用一次
# 0 1 * * * /usr/local/bin
#########################################################

#!/bin/bash
source /etc/profile

echo password:$PGPASSWORD
DB_URL=$1
DB_NAME=$2
backup_path='/var/vocp/backup/db'
if  [ ! -d ${backup_path} ];then
    mkdir -p  ${backup_path}
fi

pg_dump -i -h ${DB_URL} -p 5432 -U postgres -F c -b -v -f ${backup_path}/${DB_URL}_${DB_NAME}_`date +%Y-%m-%d`.backup ${DB_NAME}