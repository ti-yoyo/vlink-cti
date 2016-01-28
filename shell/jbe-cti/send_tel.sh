##########################################################
#
#发送电话
#
#########################################################

#!/bin/bash
log_file="/var/log/ccic/send_tel.log"
now=`date  +"%Y-%m-%d %H:%M:%S"`

#mobile
#echo "$1"
alarm_tel=$1
#msg
#echo $2
msg=$2

result_xa=`curl -s -m 10 "http://xa.ccic2.com/interface/entrance/OpenInterfaceEntrance" -d "interfaceType=webCall&enterpriseId=3000234&userName=alert&pwd=287178f04d1be58dd1f30422933b5744&customerNumber=${alarm_tel}&remoteIp=127.0.0.1&cookie=1&sync=0&paramTypes=2&ivrId=1707&paramNames=alert_content&alert_content=${msg}"`
ok_xa=`echo "${result_xa}" |grep '提交成功' |wc -l`
if [ ${ok_xa} -eq 1 ] ;then
    echo "send alarm tel success by xa.ccic2.com"
else
    result_puruan=`curl -s -m 10 "http://puruan.ccic2.com/interface/entrance/OpenInterfaceEntrance" -d "interfaceType=webCall&enterpriseId=3001821&userName=alert&pwd=287178f04d1be58dd1f30422933b5744&customerNumber=${alarm_tel}&remoteIp=127.0.0.1&cookie=1&sync=0&paramTypes=2&ivrId=220&paramNames=alert_content&alert_content=${msg}"`

    ok_puruan=`echo "${result_puruan}" |grep '提交成功' |wc -l`
    if [ ${ok_puruan} -eq 1 ] ;then
        echo "send alarm tel success by puruan.ccic2.com"
    else
        result_bj_out_2=`curl -s -m 10 "http://bj-out-2.ccic2.com/interface/entrance/OpenInterfaceEntrance" -d "interfaceType=webCall&enterpriseId=3001822&userName=alert&pwd=287178f04d1be58dd1f30422933b5744&customerNumber=${alarm_tel}&remoteIp=127.0.0.1&cookie=1&sync=0&paramTypes=2&ivrId=1126&paramNames=alert_content&alert_content=${msg}"`
        ok_bj_out_2=`echo "${result_bj_out_2}" |grep '提交成功' |wc -l`
        if [ ${ok_bj_out_2} -eq 1 ];then
            echo "send alarm tel success by bj-out-2.ccic2.com"
        else
            echo "${now} send alarm tel all faild ! $1 $2" >> ${log_file}
        fi
    fi
fi
