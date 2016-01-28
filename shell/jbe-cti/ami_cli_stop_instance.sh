#!/bin/bash

instance_status=(running running running)            #实例状态默认值
sleep_time=46                                        #2倍instance关闭的时间，测试一个实例的关闭时间为23秒
NOTIFY="anjb@ti-net.com.cn"                          #邮件通知列表

instance_num=`echo $#`
instance_array=($@)
for((i=0;i<$instance_num;i++));do
echo ${instance_array[$i]}
    current_instance_status=`aws ec2 stop-instances --instance-ids ${instance_array[$i]} | grep Name | head -1 | awk -F '"' '{print $4}'`
    echo $current_instance_status
    if test "X${current_instance_status}" != "X";then
        instance_status[${i}]=$current_instance_status
    else
        instance_status[${i}]=running
    fi
    echo ${instance_status[@]}
done
sleep ${sleep_time}
watch_continue=0
for((i=0;i<$instance_num;i++));do
    echo ${instance_array[$i]}
    current_instance_status=`aws ec2 stop-instances --instance-id ${instance_array[$i]} | grep Name | head -1 | awk -F '"' '{print $4}'`
    echo $current_instance_status
    if test "X${current_instance_status}" != "X";then
        instance_status[${i}]=$current_instance_status
    else
        instance_status[${i}]=running
    fi
    echo ${instance_status[@]}
    if test "X${current_instance_status}" != "Xstopped";then
        watch_continue=1
    fi
done
if test "X${watch_continue}" == "X1"
then
    sleep ${sleep_time}
    for((i=0;i<$instance_num;i++));do
        echo ${instance_array[$i]}
        current_instance_status=`aws ec2 stop-instances --instance-id ${instance_array[$i]} | grep Name | head -1 | awk -F '"' '{print $4}'`
        echo $current_instance_status
        if test "X${current_instance_status}" == "X";then
            instance_status[${i}]=$current_instance_status
        else
            instance_status[${i}]=running
        fi
        echo ${instance_status[@]}
        if test "X${current_instance_status}" != "Xstopped";then
            echo "warnning!!!fail to stop instance ${instance_array[$i]} at $(date +%Y-%m-%d" "%H:%M:%S)!" | mail -s "fail to stop instance ${instance_array[$i]}!" $NOTIFY
        fi
    done
fi
