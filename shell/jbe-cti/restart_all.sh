##########################################################
#
# 重启jbe cti服务
# 每个实例不同时间动态重启
#########################################################

#!/bin/bash
. /etc/profile
redis_path=/usr/local/bin
BOSS_URL=http://vocp-boss-1389722819.cn-north-1.elb.amazonaws.com.cn
externalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/public-ipv4"`
internalIp=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/local-ipv4"`
mac=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/mac"`
instanceId=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/instance-id"`
local_ip=`ifconfig -a | grep inet | grep -v 127.0.0.1 | grep -v inet6 | awk '{print $2}' | awk -F ':' '{print $2}' | head -n 1`
ELB=vocp-web
vocp_ip=vocp-redis-001.jkffod.0001.cnn1.cache.amazonaws.com.cn
instanceId=`curl -s -m 10 "http://169.254.169.254/latest/meta-data/instance-id"`
echo ELB=$ELB instanceId=${instanceId}
index=`cat /var/run/systemName | awk -F '-' '{print $2}'`
echo index=$index
wait_second=`expr $index \* 300`
echo wait_second=$wait_second
sleep $wait_second

/usr/local/bin/ami_cli_deregister_instance.sh ${ELB} ${instanceId}

retry=0
while :
do
    echo "curl -m 20 \"${BOSS_URL}/ec2InterfaceAction/registerJBE.action\" -d \"internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=0\"">/tmp/restart_all.log
    curl_res=`curl -m 20 "${BOSS_URL}/ec2InterfaceAction/registerJBE.action" -d "internalIp=${internalIp}&externalIp=${externalIp}&mac=${mac}&instanceId=${instanceId}&status=0"`
    #curl_res='{"result":"0","systemName":"jbe-1"}'
    #curl_res='{"result":"-1" }'
    echo "${curl_res}">>/tmp/restart_all.log
    result=`echo ${curl_res}| /usr/local/bin/json.sh |grep '\["result"\]'| awk '{printf $2}'|sed 's/^"//;s/"$//'`
    echo "result:[${result}]">>/tmp/restart_all.log

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
done


sleep 60
/usr/local/bin/restart_asterisk >/dev/null
su - tomcat -c /usr/local/bin/restart_catalina
sleep 60

/usr/local/bin/ami_cli_register_instance.sh ${ELB} ${instanceId}
