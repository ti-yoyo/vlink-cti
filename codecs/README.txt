
文件			格式
codec_g723.so 		X86_64平台，core2 CPU，需要sse4的支持
codec_g729.so 		X86_64平台，core2 CPU，需要sse4的支持


使用方法：

	a:放到/usr/lib/asterisk/modules 目录下并给予777的权限。
	b:在asterisk的CLI命令行中使用 module load codec_g723.so 加载
	c:查看是否已经加载成功 ; 在asterisk的CLI命令行中使用core show translation  查看

配置文件：

G.723.1 的发送码率是在Asterisk codecs.conf file文件中来配置的，接收码率无法配置，配置的样例如下:
[g723]
; 6.3Kbps stream, default
sendrate=63
; 5.3Kbps
;sendrate=53

请在 sip.conf 的全局配置部分增加启用配置：

例如：
allow=g729