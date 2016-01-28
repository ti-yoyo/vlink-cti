安装智能挂断模块说明
Author：liucl
Modify date：2014-04-29

该功能用于CCIC2外呼。当外呼时，通过识别客户侧的回铃音来识别出座席的状态，客户通道返回提示音如忙、通话中、空号、停机、无法接通等几种情况主动将客户通道挂断，以节约通道资源，并提高呼叫效率。



二、安装asr模块
天润: http://172.16.203.9/svn/CCIC/branches/VOCP/trunk/CCIC2/cti/asr/tinet
svn co http://172.16.203.9/svn/CCIC/branches/VOCP/trunk/CCIC2/cti/asr /home/svn/asr


创建存放log的目录
mkdir -p /var/log/ccic/asr



安装天润自己客户端：
cp -f /home/CCIC/cti/asr/tinet/libmatch_pcm.so /usr/local/lib/
ldconfig -v|grep match_pcm
cd /home/CCIC/cti/asr/tinet/
gcc -o match_pcm match_pcm.c -lmatch_pcm

cp -f /home/CCIC/cti/asr/tinet/audio_file_name /usr/local/bin/
cp -f /home/CCIC/cti/asr/tinet/record /usr/local/bin/
cp -f /home/CCIC/cti/asr/tinet/match_pcm /usr/local/bin/


chmod +x /usr/local/bin/match_pcm




四、日志
生成的日志保存在/var/log/ccic/asr目录内

5.tinet产生的日志


五、注意事项
该文件会读取/var/nfs/vocp/voices/ASR目录内的录音文件。如某个企业号使用本功能，需要在营帐中将此开关打开，才能在ASR目录内生成呼叫客户侧的录音文件。
