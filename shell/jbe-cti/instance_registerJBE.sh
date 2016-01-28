#!/bin/bash
#
ALARM_SMS=18612015255,13426307922
ALARM_TEL=18612015255
ALARM_EMAIL=vocp.list@ti-net.com.cn

BOSS_URL=http://vocp-boss-1389722819.cn-north-1.elb.amazonaws.com.cn
###################################################
externalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/public-ipv4"`
internalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/local-ipv4"`
mac=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/mac"`
instanceId=`curl -s -m 20 "http://169.254.169.254/latest/meta-data/instance-id"`

echo "curl -m 10 \"${BOSS_URL}/ec2InterfaceAction/registerJBE.action\" -d \"internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=1\"">/tmp/instance_registerJBE.log
curl_res=`curl -m 20 "${BOSS_URL}/ec2InterfaceAction/registerJBE.action" -d "internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=1"`
echo "${curl_res}">>/tmp/instance_registerJBE.log
result=`echo ${curl_res}| /usr/local/bin/json.sh |grep '\["result"\]'| awk '{printf $2}'|sed 's/^"//;s/"$//'`
echo "result:[${result}]">>/tmp/instance_registerJBE.log

if [ "x${result}" = "x0" ]; then
    echo "`date +%Y-%m-%d@%H:%M:%S` 注册成功">>/tmp/instance_registerJBE.log    

    sleep 20
else
    echo "`date +%Y-%m-%d@%H:%M:%S` 注册失败">>/tmp/instance_registerJBE.log
    if [ "x${ALARM_SMS}" != "x" ]; then
        echo "发送短信告警"
        /usr/local/bin/send_sms.sh ${ALARM_SMS} "VOCP实例注册失败 [internalIp=${internalIp} externalIp=${externalIp} mac=${mac} instanceId=${instanceId}]"
    fi
    if [ "x${ALARM_TEL}" != "x" ]; then
        echo "发送电话告警"
        /usr/local/bin/send_tel.sh ${ALARM_TEL} "VOCP实例注册失败 内网ip:${internalIp} 公网ip:${externalIp}"
    fi
    if [ "x${ALARM_EMAIL}" != "x" ]; then
        echo "发送邮件告警"
        /usr/local/bin/send_mail.sh ${ALARM_EMAIL} "VOCP实例注册失败" "[internalIp=${internalIp} externalIp=${externalIp} mac=${mac} instanceId=${instanceId}]"
    fi
fi


local_ip=`ifconfig -a | grep inet | grep -v 127.0.0.1 | grep -v inet6 | awk '{print $2}' | awk -F ':' '{print $2}' | head -n 1`
instance_name=Vlink_${local_ip}
python /usr/local/bin/host_add.py -a ${instance_name} ${local_ip}

