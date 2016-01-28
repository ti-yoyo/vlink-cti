#!/bin/bash
sleep_time=1                                        #2倍instance启动的时间，测试一个实例的启动时间为15秒
NOTIFY="anjb@ti-net.com.cn"                          #邮件通知列表

instance_num=`echo $#`
instance_num=$(($instance_num-1))
argv_cache=($@)
elb_name=${argv_cache[0]}
for((i=0;i<$instance_num;i++))
do
    num=$(($i+1))
    instance_array[${i}]=${argv_cache[${num}]}
done
for((i=0;i<$instance_num;i++));do
    echo ${instance_array[$i]}
    aws elb register-instances-with-load-balancer  --load-balancer-name ${elb_name} --instances ${instance_array[$i]}
done
sleep ${sleep_time}
watch_continue=0
register_instances=`aws elb describe-load-balancers --load-balancer-name ${elb_name} | grep InstanceId | awk -F '\"' '{print $4}'`
for((i=0;i<$instance_num;i++));do
    register_status=`echo $register_instances | grep ${instance_array[$i]}`
    if test "X${register_status}" == "X";then
        echo "fail to register instances ${instance_array[$i]} to ${elb_name}"
        watch_continue=1
        aws elb register-instances-with-load-balancer  --load-balancer-name ${elb_name} --instances ${instance_array[$i]}
    fi
done
sleep ${sleep_time}
for((i=0;i<$instance_num;i++));do
    register_status=`echo $register_instances | grep ${instance_array[$i]}`
    if test "X${register_status}" == "X";then
        echo "fail to register instances ${instance_array[$i]} to ${elb_name} at $(date +%Y-%m-%d" "%H:%M:%S)!" | mail -s "fail to register instances" NOTIFY
    fi
done

