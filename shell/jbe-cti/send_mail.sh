##########################################################
#
#发送email
#
#########################################################

#!/bin/bash
export LANG=en_US.UTF-8
export LC_CTYPE=en_US.UTF-8
export LANG=en_US.UTF-8
export LC_CTYPE=en_US.UTF-8
export LC_NUMERIC="en_US.UTF-8"
export LC_TIME="en_US.UTF-8"
export LC_COLLATE="en_US.UTF-8"
export LC_MONETAR=Y"en_US.UTF-8"
export LC_MESSAGES="en_US.UTF-8"
export LC_PAPER="en_US.UTF-8"
export LC_NAME="en_US.UTF-8"
export LC_ADDRESS="en_US.UTF-8"
export LC_TELEPHONE="en_US.UTF-8"
export LC_MEASUREMENT="en_US.UTF-8"
export LC_IDENTIFICATION="en_US.UTF-8"
export LC_ALL=

log_file="/var/log/ccic/send_mail.log"
now=`date  +"%Y-%m-%d %H:%M:%S"`

#mobile
#echo "$1"
alarm_mail=$1
#title
#echo $2
title=$2
#msg
#echo $3
msg=$3

echo "${msg}" | mail -s "${title}" ${alarm_mail}
echo "${now} send mail to [${alarm_mail}] title=[${title}] msg=[${msg}]"
