 ##########################################################
#
#自动升级jbe
#
#########################################################

#!/bin/bash
#

SVN_PATH=/home/svn/VOCP/CCIC2/deploy/jbe/war/

revision=$1

cd ${SVN_PATH}

if [ "${revision}" = "" ]; then
	svn update
else
	svn update -r ${revision}
fi
rm -rf /tmp/ccic2.war

svn export ccic2.war /tmp/ccic2.war

rm -rf /usr/local/apache-tomcat-8.0.9/webapps/ROOT/*
mv /tmp/ccic2.war/* /usr/local/apache-tomcat-8.0.9/webapps/ROOT/
ln -s /var/nfs/vocp/voices /usr/local/apache-tomcat-8.0.9/webapps/ROOT/voices
ln -s /var/nfs/vocp/logo /usr/local/apache-tomcat-8.0.9/webapps/ROOT/images/logo
ln -s /var/nfs/vocp/KBUserfiles /usr/local/apache-tomcat-8.0.9/webapps/ROOT/images/KBUserfiles
su - tomcat -c /usr/local/bin/restart_catalina
