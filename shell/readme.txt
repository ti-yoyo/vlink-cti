本文档主要介绍shell目录内的所有文件。介绍的内容包含：文件的作用、运行方式及运行的平台等。
若在本目录添加新的shell脚本，请按照规则更新此文档。

按照不同平台进行分类：

JBE-CTI平台上：
1). log_handle.sh
	作    用：用于NFS平台上日志文件的分割转储或者清理。
	运行方式：定时启动；
2).  remove_old_wav.sh
 	作    用：移除NFS服务器上备份的7天前那一天的wav文件；
 	运行方式：利用cron，每天执行一次；

JBE平台  --------------------------------------
1). restart_catalina
	作    用：重启catalina
	运行方式：设置在crontab中定时执行，默认情况下，每天早上03:40执行一次；
2). safe_catalina
	作    用：监控catalina进程，如果catalina进程宕掉，本脚本会重新启动catalina；
	运行方式：不直接运行，由restart_catalina调用；
1).  restart_asterisk
	 作    用：重启asterisk
	 运行方式：设置在crontab中，每天早上定时执行一次；
2).  move_to_voices.sh
	 作    用：将生成的wav录音文件转成mp3到/var/local/voices/mp3目录里，上传到s3.并保存一份备份文件到/var/nfs/vocp/voices/backup/目录内；
	 运行方式：被dialplan AGI调用
3).  rsync_moh_ivr.sh
	 作    用：将/var/nfs/voices/moh和/var/nfs/voices/ivr_voice目录里的内容分别同步到/var/lib/moh和/var/lib/ivr_voice目录内；因为，asterisk使用的/var/lib/moh和/var/lib/ivr_voice目录里的语音文件；
	 运行方式：后台运行，开机启动。


DB备份  -----------------------------------------
1). dump_db.sh
	作    用：备份RDS数据库；
	运行方式：利用cron，每周晚上执行一次；多个实例分开执行；