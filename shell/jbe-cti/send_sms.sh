##########################################################
#
#发送短信
#
#########################################################

#!/bin/bash
log_file="/var/log/ccic/send_sms.log"
now=`date  +"%Y-%m-%d %H:%M:%S"`

#mobile
#echo "$1"
alarm_sms_list=$1
#msg
#echo $2
msg=$2

echo "${now} send alarm tel=[${alarm_sms_list}] msg=[${msg}]" >> ${log_file}

result_xa_sms=`curl -s -m 10 "http://xa.ccic2.com/interface/sms/SendSms" -d "enterpriseId=3000234&userName=alert&pwd=ee487ce567c8fa237ba1888eb17440a8&seed=aaa&type=8&mobile=${alarm_sms_list}&customerName=VOCP_instance_startup.sh&msg=${msg}"`
#echo "xa result:[${result_xa_sms}]"
ok_xa_sms=`echo ${result_xa_sms} |grep '提交成功' |wc -l`
if [ ${ok_xa_sms} -eq 1 ] ;then
    echo "send alarm sms success by xa.ccic2.com"
else
    result_puruan_sms=`curl -s -m 10 "http://puruan.ccic2.com/interface/sms/SendSms" -d "enterpriseId=3001821&userName=alert&pwd=ee487ce567c8fa237ba1888eb17440a8&seed=aaa&type=8&mobile=${alarm_sms_list}&customerName=VOCP_instance_startup.sh&msg=${msg}"`
    echo "puruan result:[${result_puruan_sms}]"
    ok_puruan_sms=`echo ${result_puruan_sms} |grep '提交成功' |wc -l`
    if [ ${ok_puruan_sms} -eq 1 ] ;then
        echo "send alarm sms success by puruan.ccic2.com"
    else
        result_bj_out_2_sms=`curl -s -m 10 "http://bj-out-2.ccic2.com/interface/sms/SendSms" -d "enterpriseId=3001822&userName=alert&pwd=ee487ce567c8fa237ba1888eb17440a8&seed=aaa&type=8&mobile=${alarm_sms_list}&customerName=VOCP_instance_startup.sh&msg=${msg}"`
        echo "bj_out_2 result:[${result_bj_out_2_sms}]"
	ok_bj_out_2_sms=`echo ${result_bj_out_2_sms} |grep '提交成功' |wc -l`
        if [ ${ok_bj_out_2_sms} -eq 1 ] ;then 
            echo "send alarm sms success by bj-out-2.ccic2.com"
        else
            echo "${now} send alarm sms all faild ! $1 $2" >> ${log_file}
        fi
    fi          
fi
