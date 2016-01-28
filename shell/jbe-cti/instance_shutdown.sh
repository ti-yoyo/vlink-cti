##########################################################
#
#实例注销动作
#
#########################################################

#!/bin/bash
#


BOSS_URL=http://vocp-boss-1389722819.cn-north-1.elb.amazonaws.com.cn
ALARM_SMS=13426307922
ALARM_TEL=13426307922
ALARM_EMAIL=vocp.list@ti-net.com.cn
#############
externalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/public-ipv4"`
internalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/local-ipv4"`
mac=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/mac"`
instanceId=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/instance-id"`

retry=0
while :
do
    echo "curl -m 10 \"${BOSS_URL}/ec2InterfaceAction/unregisterJBE.action\" -d \"instanceId=${instanceId}\"">/tmp/instance_shutdown.log
    curl_res=`curl -m 10 "${BOSS_URL}/ec2InterfaceAction/unregisterJBE.action" -d "instanceId=${instanceId}"`
    #curl_res='{"result":"0"}'
    #curl_res='{"result":"-1" }'
    echo "$curl_res">>/tmp/instance_shutdown.log
    result=`echo ${curl_res}| /usr/local/bin/json.sh |grep '\["result"\]'| awk '{printf $2}'|sed 's/^"//;s/"$//'`
    echo "result:[${result}]">>/tmp/instance_shutdown.log

    if [ "x${result}" = "x0" ]; then
            break
    else
        retry=`expr ${retry} + 1`
        if [ ${retry} -gt 2 ]; then
            break
        else
            sleep 1
        fi
    fi
done

if [ "x${result}" = "x0" ]; then
    echo "取消注册成功"
    echo "`date +%Y-%m-%d@%H:%M:%S` 取消注册成功 [internalIp=${internalIp} externalIp=${externalIp} mac=${mac} instanceId=${instanceId}]" >>/var/log/ccic/instance_shutdown.log
else
    echo "取消注册失败"
    echo "`date +%Y-%m-%d@%H:%M:%S` ${instanceId} 取消注册失败" >>/var/log/ccic/instance_shutdown.log
    if [ "x${ALARM_SMS}" != "x" ]; then
	echo "发送短信告警"
        /usr/local/bin/send_sms.sh ${ALARM_SMS} "VOCP实例取消注册失败 [internalIp=${internalIp} externalIp=${externalIp} mac=${mac} instanceId=${instanceId}]"
    fi
    if [ "x${ALARM_TEL}" != "x" ]; then
	echo "发送电话告警"
        /usr/local/bin/send_tel.sh ${ALARM_TEL} "VOCP实例取消注册失败 内网ip:${internalIp} 公网ip:${externalIp}"
    fi
    if [ "x${ALARM_EMAIL}" != "x" ]; then
	echo "发送邮件告警"
        /usr/local/bin/send_mail.sh ${ALARM_EMAIL} "VOCP实例注册取消失败" "[internalIp=${internalIp} externalIp=${externalIp} mac=${mac} instanceId=${instanceId}]"
    fi
fi


