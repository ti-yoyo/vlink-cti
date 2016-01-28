#!/bin/sh
# vim:textwidth=80:tabstop=4:shiftwidth=4:smartindent:autoindent
#
# Write : anjb@ti-net.com.cn

location="VOCP"
alarm_type="mail,sms,tel"

ALARM_SMS="13426307922;18612015255;18500136173"
ALARM_TEL="13426307922,18612015255,18500136173"
#ALARM_EMAIL=anjb@ti-net.com.cn
ALARM_EMAIL=vocp.list@ti-net.com.cn


internalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/local-ipv4"`
externalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/public-ipv4"`

DIED_TIME=`date  +"%F %T.%N"`
log_file="/var/log/ccic/monitor_cti.log.$(date +%Y%m%d)"

ami_con_count=`/usr/sbin/asterisk -rx'manager show connected' |grep -P 'action|manager' |wc -l`
echo " ------------- check the port  ${DIED_TIME}  the ami_con_count : ${ami_con_count} "  >> ${log_file}
echo "start"
if [ ${ami_con_count} -eq 0 ] ;then
    if [ "x${ALARM_SMS}" != "x" ]; then
	echo "发送短信告警"
        /usr/local/bin/send_sms.sh ${ALARM_SMS} "VOCP,AMI,端口,5038无连接 [internalIp=${internalIp}&externalIp=${externalIp}]"
    fi
    if [ "x${ALARM_TEL}" != "x" ]; then
	echo "发送电话告警"
        /usr/local/bin/send_tel.sh ${ALARM_TEL} "VOCP,AMI,端口,5038无连接 内网ip:${internalIp} 公网ip:${externalIp}"
    fi
    if [ "x${ALARM_EMAIL}" != "x" ]; then
	echo "发送邮件告警"
        /usr/local/bin/send_mail.sh ${ALARM_EMAIL} "VOCP,AMI,端口,5038无连接" "[internalIp=${internalIp}&externalIp=${externalIp}]"
    fi
fi


