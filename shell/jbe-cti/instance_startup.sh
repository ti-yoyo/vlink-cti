 ##########################################################
#
#实例启动动作
#
#########################################################

#!/bin/bash
#

BOSS_URL=http://vocp-boss-1389722819.cn-north-1.elb.amazonaws.com.cn
Name_pre=vocp_
instance_principal='洪志奎'
ALARM_SMS=18612015255,13426307922
ALARM_TEL=18612015255
ALARM_EMAIL=vocp.list@ti-net.com.cn
#############
externalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/public-ipv4"`
internalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/local-ipv4"`
mac=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/mac"`
instanceId=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/instance-id"`


retry=0
while :
do
    echo "curl -m 20 \"${BOSS_URL}/ec2InterfaceAction/registerJBE.action\" -d \"internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=0\"">/tmp/instance_startup.log
    curl_res=`curl -m 20 "${BOSS_URL}/ec2InterfaceAction/registerJBE.action" -d "internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=0"`
    #curl_res='{"result":"0","systemName":"jbe-1"}'
    #curl_res='{"result":"-1" }'
    echo "${curl_res}">>/tmp/instance_startup.log
    result=`echo ${curl_res}| /usr/local/bin/json.sh |grep '\["result"\]'| awk '{printf $2}'|sed 's/^"//;s/"$//'`
    echo "result:[${result}]">>/tmp/instance_startup.log

    if [ "x${result}" = "x0" ]; then
            break
    else
        retry=`expr ${retry} + 1`
        if [ ${retry} -gt 10 ]; then
            break
        else
            sleep 1
        fi
    fi
    aws ec2 create-tags --resources ${instanceId} --tags Key=名称,Value=${Name_pre}${result} Key=负责人,Value=${instance_principal} Key=维护人,Value=${instance_principal}
done

if [ "x${result}" = "x0" ]; then
    echo "`date +%Y-%m-%d@%H:%M:%S` 注册成功">>/tmp/instance_startup.log
    #启动tomcat
    system_name=`echo ${curl_res}| /usr/local/bin/json.sh |grep '\["systemName"\]'| awk '{printf $2}'|sed 's/^"//;s/"$//'`
    echo "`date +%Y-%m-%d@%H:%M:%S` system_name:[${system_name}]">>/tmp/instance_startup.log
    sed -i "/^systemname/csystemname = ${system_name}" /etc/asterisk/asterisk.conf
    sed -i "/^externaddr=/cexternaddr=${externalIp}" /etc/asterisk/sip.conf
    echo "${system_name}" >/var/run/systemName
    mkdir -p /var/nfs/logs/${system_name}/ccic
    mkdir -p /var/nfs/logs/${system_name}/tomcat
    mkdir -p /var/nfs/logs/${system_name}/asterisk
    mkdir -p /var/nfs/logs/${system_name}/asterisk/cdr-csv
    mkdir -p /var/nfs/logs/${system_name}/sip

    mkdir -p /var/nfs/logs/${system_name}/tomcat/catalina_bak

    chown tomcat.tomcat -Rf /var/nfs/logs/${system_name}/ccic
    chown tomcat.tomcat -Rf /var/nfs/logs/${system_name}/tomcat
    chown tomcat.tomcat -Rf /var/nfs/logs/${system_name}/asterisk

    chmod 777 -Rf /var/nfs/logs/${system_name}/ccic
    chmod 777 -Rf /var/nfs/logs/${system_name}/tomcat
    chmod 777 -Rf /var/nfs/logs/${system_name}/asterisk
    chmod 777 -Rf /var/nfs/logs/${system_name}/sip
    if [ ! -h "/var/log/ccic" ]; then
        #不是符号文件
        rm -rf /var/log/ccic
    else
        rm -f /var/log/ccic
    fi

    if [ ! -h "/usr/local/apache-tomcat-8.0.9/logs" ]; then
        #不是符号文件
        rm -rf /usr/local/apache-tomcat-8.0.9/logs
    else
        rm -f /usr/local/apache-tomcat-8.0.9/logs
    fi

    if [ ! -h "/var/log/asterisk" ]; then
        #不是符号文件
        rm -rf /var/log/asterisk
    else
        rm -f /var/log/asterisk
    fi
    if [ ! -h "/var/log/sip" ]; then
        #不是符号文件
        rm -rf /var/log/sip
    else
        rm -f /var/log/sip
    fi
    ln -s /var/nfs/logs/${system_name}/ccic /var/log/ccic
    ln -s /var/nfs/logs/${system_name}/tomcat /usr/local/apache-tomcat-8.0.9/logs
    ln -s /var/nfs/logs/${system_name}/asterisk /var/log/asterisk
    ln -s /var/nfs/logs/${system_name}/sip /var/log/sip
    chown tomcat.tomcat -Rf /var/log/ccic
    chown tomcat.tomcat -Rf /usr/local/apache-tomcat-8.0.9/logs
    chmod 777 -Rf /var/log/ccic
    chmod 777 -Rf /usr/local/apache-tomcat-8.0.9/logs
    chmod 777 -Rf /var/log/sip
    chmod 777 -Rf /var/log/asterisk

    #启动业务asterisk和tomcat
    /usr/local/bin/restart_asterisk
    su - tomcat -c /usr/local/bin/restart_catalina
    nohup /usr/local/bin/remove_asr.sh >/dev/null 2>&1 &
    nohup /usr/local/bin/move_to_voices.sh >/dev/null 2>&1 &

    sleep 20
else
    echo "`date +%Y-%m-%d@%H:%M:%S` 注册失败">>/tmp/instance_startup.log
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


