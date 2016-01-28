#!/bin/sh

# 同步备份/var/nfs/voices/ivr_voice 到 /var/lib/ivr_voice
# 同步备份/var/nfs/voices/moh 到 /var/lib/moh
# 开启命令：nohup /usr/local/bin/rsync_moh_ivr.sh > /dev/null 2>&1 &
# 并把该命令添加到/etc/rc.d/rc.local中

if [ ! -d /var/lib/ivr_voice ]; then
    mkdir -p /var/lib/ivr_voice
fi
if [ ! -d /var/lib/moh ]; then
    mkdir -p /var/lib/moh
fi
if [ ! -d /var/log/ccic ]; then
    mkdir -p /var/log/ccic
fi

do_rsync(){
    echo `date +%Y-%m-%d@%H:%M:%S`
    rsync -av --delete /var/nfs/vocp/voices/ivr_voice/ /var/lib/ivr_voice/
    rsync -av --delete /var/nfs/vocp/voices/moh/ /var/lib/moh/
}

while :
do
    do_rsync > /var/log/ccic/rsync_moh_ivr.log
    # 每10秒钟单向同步一次
    sleep 10
done
