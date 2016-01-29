/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief RADIUS CEL Support
 * \author Philippe Sultan
 * The Radius Client Library - http://developer.berlios.de/projects/radiusclient-ng/
 *
 * \arg See also \ref AstCEL
 * \ingroup cel_drivers
 */

/*** MODULEINFO
	<depend>radius</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Rev$")

#ifdef FREERADIUS_CLIENT
#include <freeradius-client.h>
#else
#include <radiusclient-ng.h>
#endif

#include "asterisk/channel.h"
#include "asterisk/cel.h"
#include "asterisk/lock.h"
#include <dirent.h>
#include <sys/signal.h>
#include <signal.h>
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/utils.h"
#include "asterisk/options.h"

/*! ISO 8601 standard format */
#define DATE_FORMAT "%Y-%m-%d %T %z"

#define VENDOR_CODE           22736

enum {
	PW_AST_ACCT_CODE =    101,
	PW_AST_CIDNUM =       102,
	PW_AST_CIDNAME =      103,
	PW_AST_CIDANI =       104,
	PW_AST_CIDRDNIS =     105,
	PW_AST_CIDDNID =      106,
	PW_AST_EXTEN =        107,
	PW_AST_CONTEXT =      108,
	PW_AST_CHANNAME =     109,
	PW_AST_APPNAME =      110,
	PW_AST_APPDATA =      111,
	PW_AST_EVENT_TIME =   112,
	PW_AST_AMA_FLAGS =    113,
	PW_AST_UNIQUE_ID =    114,
	PW_AST_USER_NAME =    115,
	PW_AST_LINKED_ID =    116,
	PW_AST_EXTRA =        117,
};

enum {
	/*! Log dates and times in UTC */
	RADIUS_FLAG_USEGMTIME = (1 << 0),
	/*! Log Unique ID */
	RADIUS_FLAG_LOGUNIQUEID = (1 << 1),
	/*! Log User Field */
	RADIUS_FLAG_LOGUSERFIELD = (1 << 2)
};

static char *cel_config = "cel.conf";

#ifdef FREERADIUS_CLIENT
static char radiuscfg[PATH_MAX] = "/etc/radiusclient/radiusclient.conf";
#else
static char radiuscfg[PATH_MAX] = "/etc/radiusclient-ng/radiusclient.conf";
#endif

static struct ast_flags global_flags = { RADIUS_FLAG_USEGMTIME | RADIUS_FLAG_LOGUNIQUEID | RADIUS_FLAG_LOGUSERFIELD };

static rc_handle *rh = NULL;

#define RADIUS_BACKEND_NAME "CEL Radius Logging"

#define ADD_VENDOR_CODE(x,y) (rc_avpair_add(rh, send, x, (void *)y, strlen(y), VENDOR_CODE))

static pthread_t monitor_thread = AST_PTHREADT_NULL;
AST_MUTEX_DEFINE_STATIC(monlock);
static char cdr_dir_tmp[PATH_MAX] = "/tmp";
static char cdr_directory[PATH_MAX] = "/var/lib/cdr";

static int build_radius_record(VALUE_PAIR **send, struct ast_cel_event_record *record)
{
	int recordtype = PW_STATUS_STOP;
	struct ast_tm tm;
	char timestr[128];
	char *amaflags;

	if (!rc_avpair_add(rh, send, PW_ACCT_STATUS_TYPE, &recordtype, 0, 0)) {
		return -1;
	}
	/* Account code */
	if (!ADD_VENDOR_CODE(PW_AST_ACCT_CODE, record->account_code)) {
		return -1;
	}
	/* Source */
	if (!ADD_VENDOR_CODE(PW_AST_CIDNUM, record->caller_id_num)) {
		return -1;
	}
	/* Destination */
	if (!ADD_VENDOR_CODE(PW_AST_EXTEN, record->extension)) {
		return -1;
	}
	/* Destination context */
	if (!ADD_VENDOR_CODE(PW_AST_CONTEXT, record->context)) {
		return -1;
	}
	/* Caller ID */
	if (!ADD_VENDOR_CODE(PW_AST_CIDNAME, record->caller_id_name)) {
		return -1;
	}
	/* Caller ID ani */
	if (!ADD_VENDOR_CODE(PW_AST_CIDANI, record->caller_id_ani)) {
		return -1;
	}
	/* Caller ID rdnis */
	if (!ADD_VENDOR_CODE(PW_AST_CIDRDNIS, record->caller_id_rdnis)) {
		return -1;
	}
	/* Caller ID dnid */
	if (!ADD_VENDOR_CODE(PW_AST_CIDDNID, record->caller_id_dnid)) {
		return -1;
	}
	/* Channel */
	if (!ADD_VENDOR_CODE(PW_AST_CHANNAME, record->channel_name)) {
		return -1;
	}
	/* Last Application */
	if (!ADD_VENDOR_CODE(PW_AST_APPNAME, record->application_name)) {
		return -1;
	}
	/* Last Data */
	if (!ADD_VENDOR_CODE(PW_AST_APPDATA, record->application_data)) {
		return -1;
	}
	/* Extra */
	
	int left_len=strlen(record->extra);
        char *left_value = (char *)record->extra;
        int send_len;
        while(left_len > 0){
               if(left_len > 240){
                    send_len = 240;
                }else{
                    send_len = left_len; 
                }
                if(!rc_avpair_add(rh, send, PW_AST_EXTRA, left_value, send_len, VENDOR_CODE )) {
			return -1;
                }
                left_len-=send_len;
                left_value+=send_len;
        }
	
	/* Event Time */
	ast_localtime(&record->event_time, &tm,
		ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL);
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT, &tm);
	if (!rc_avpair_add(rh, send, PW_AST_EVENT_TIME, timestr, strlen(timestr), VENDOR_CODE)) {
		return -1;
	}
	/* AMA Flags */
	amaflags = ast_strdupa(ast_channel_amaflags2string(record->amaflag));
	if (!rc_avpair_add(rh, send, PW_AST_AMA_FLAGS, amaflags, strlen(amaflags), VENDOR_CODE)) {
		return -1;
	}
	if (ast_test_flag(&global_flags, RADIUS_FLAG_LOGUNIQUEID)) {
		/* Unique ID */
		if (!ADD_VENDOR_CODE(PW_AST_UNIQUE_ID, record->unique_id)) {
			return -1;
		}
	}
	/* LinkedID */
	if (!ADD_VENDOR_CODE(PW_AST_LINKED_ID, record->linked_id)) {
		return -1;
	}
	/* Setting Acct-Session-Id & User-Name attributes for proper generation
	   of Acct-Unique-Session-Id on server side */
	/* Channel */
	if (!rc_avpair_add(rh, send, PW_USER_NAME, (void *)record->channel_name,
			strlen(record->channel_name), 0)) {
		return -1;
	}
	return 0;
}

static VALUE_PAIR *get_avp(const char *file)
{
	FILE *in;
	char tmp[256];
	VALUE_PAIR *avp = NULL;
	VALUE_PAIR *avp_head = NULL;
	VALUE_PAIR *avp_tmp = NULL;
	int len = 0;

	if((in=fopen(file,"r")) != NULL) {
		while(!feof(in)){
			memset(tmp,0,sizeof(tmp));
			if(!fgets(tmp,sizeof(tmp),in))
				break;
			avp = (VALUE_PAIR *)ast_malloc(sizeof(VALUE_PAIR));
			if(avp == NULL){
				return NULL;
			}
			if(avp_head == NULL){
				avp_head = avp;
			}
			ast_copy_string(avp->name,tmp,strlen(tmp));
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			avp->attribute = atoi(tmp);
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			avp->type = atoi(tmp);
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			if(avp->type == 0) {
				len = strlen(tmp)-1;
				tmp[len] = '\0';
				memcpy(avp->strvalue,tmp,strlen(tmp));
				avp->lvalue = strlen(tmp);
			}else{
				avp->lvalue = atoi(tmp);
			}
			avp->next = NULL;
			if(avp_tmp != NULL){
				avp_tmp->next = avp;
			}
			avp_tmp = avp;
		}
		fclose(in);
	}

	return avp_head;
}

/*! save avp to file if failed sending to radius server 
 *  * add by liucl
 *   */
static int save_avp_to_file(const char *avp_file, const char *file_name, VALUE_PAIR *avp)
{
	int result = 0;
	int len = 0;
	FILE *wfile;
	char tmp1[256],tmp2[256],tmp3[256];
    char full_file_name[256];
    int ret = 1; // return value of system()
	
	if((wfile=fopen(avp_file, "a+")) != NULL){
		while(avp != NULL) {
			memset(tmp1,0,sizeof(tmp1));
			memset(tmp2,0,sizeof(tmp2));
			memset(tmp3,0,sizeof(tmp3));
			if(!fwrite(avp->name,1,strlen(avp->name),wfile))
				return result;
			fwrite("\n",1,1,wfile);
			len = sprintf(tmp1,"%d",avp->attribute);
			fwrite(tmp1,1,len,wfile);
			fwrite("\n",1,1,wfile);
			len = sprintf(tmp2,"%d",avp->type);
			fwrite(tmp2,1,len,wfile);
			fwrite("\n",1,1,wfile);
			if(avp->type != 0) { // 1 | 2 | 3
				sprintf(tmp3,"%u",avp->lvalue);
				fwrite(tmp3,1,strlen(tmp3),wfile);
				fwrite("\n",1,1,wfile);
			}else { // type = 0
				fwrite(avp->strvalue,1,strlen(avp->strvalue),wfile);
				fwrite("\n",1,1,wfile);
			}
			avp = avp->next;
		}
		fclose(wfile);
		result = 1;
	}
    memset(full_file_name, 0, sizeof(full_file_name));
    snprintf(full_file_name,sizeof(full_file_name),"%s/%s", cdr_directory, file_name);
    ret = link(avp_file, full_file_name);
    ast_log(LOG_DEBUG, "ret:%d <> avp_file:%s <> full_file_name:%s\n",ret,avp_file,full_file_name);
    if(ret != 0) {
        ast_log(LOG_ERROR, "Failed to move %s\n", avp_file);
    }

	return result;
}

/*!frees all value_pairs in the list
 *  */
static void ast_avpair_free(VALUE_PAIR *pair)
{
    VALUE_PAIR *next;
    while (pair != NULL){
        next = pair->next;
        ast_free(pair);
        pair = next;
    }
}


static void *do_monitor(void *data)
{
    int result = ERROR_RC;
    char cdr_full_path[256];
    DIR *cdr_dir;
	VALUE_PAIR *resend = NULL;
	struct dirent *dir_s = NULL;
	int count = 0; // times of resend
	int wait_time = 0;
	
	for(;;){
        sleep(1); /* sleep 1 minute */
		/* Open dir */
		if((cdr_dir=opendir(cdr_directory)) == NULL){
			ast_log(LOG_ERROR,"Failed to open %s\n",cdr_directory);
		} else {
		/* open */
			while((dir_s=readdir(cdr_dir))!=NULL) {
				if(!strcmp(dir_s->d_name,".") || !strcmp(dir_s->d_name,".."))
					continue;
				memset(cdr_full_path,0,sizeof(cdr_full_path));
				snprintf(cdr_full_path,sizeof(cdr_full_path),"%s/%s",cdr_directory,dir_s->d_name);
				resend = get_avp(cdr_full_path);
				if(resend){
					result = rc_acct(rh,0,resend);
				}else{
					ast_log(LOG_ERROR, "get avp return null!");
				}
				if(result != OK_RC){
					ast_log(LOG_ERROR, "Failed to re-record Radius CDR record!\n");
					if(resend){
						ast_avpair_free(resend);
						resend = NULL;
					}
					count++;
					wait_time = count * 60; // reterval to resend cdr record: 60s, 120s,180s,240s......
					ast_log(LOG_DEBUG,"----------wait_time:%d-----------\n", wait_time);
					break;
				}else{
				ast_log(LOG_DEBUG,"----------cdr_full_path:%s send OK!-----------\n", cdr_full_path);
				wait_time = 0;
				count = 0;
				if(remove(cdr_full_path) != 0)
					ast_log(LOG_WARNING,"Failed to delete %s\n",cdr_full_path);
			}
			if(resend){
				ast_avpair_free(resend);
				resend = NULL;
			}
		}
		closedir(cdr_dir);
		}
		
		if (wait_time > 0) {
			ast_log(LOG_DEBUG,"if-wait_time>0----------wait_time:%d-----------\n", wait_time);
			sleep(wait_time);
		}
	}
	return NULL;
}

static int start_monitor(void)
{
	/* If we're supposed to be stopped -- stay stopped */
	if (monitor_thread == AST_PTHREADT_STOP)
		return 0;
	ast_mutex_lock(&monlock);
	if (monitor_thread == pthread_self()) {
		ast_mutex_unlock(&monlock);
		ast_log(LOG_WARNING, "Cannot kill myself cdr monitor\n");
		return -1;
	}
	if (monitor_thread != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(monitor_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&monitor_thread, NULL, do_monitor, NULL) < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start cdr monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);

	return 0;
}



static void radius_log(struct ast_event *event)
{
	int result = ERROR_RC;
	VALUE_PAIR *send = NULL;
	int ret = 0;
	char cdr_full_path[256];
	struct ast_cel_event_record record = {
		.version = AST_CEL_EVENT_RECORD_VERSION,
	};

	if (ast_cel_fill_record(event, &record)) {
		return;
	}
	if (build_radius_record(&send, &record)) {
		ast_debug(1, "Unable to create RADIUS record. CEL not recorded!\n");
		goto return_cleanup;
	}

	result = rc_acct(rh, 0, send);
	if (result != OK_RC) {
		ast_log(LOG_ERROR, "Failed to record Radius CEL record! unique_id=%s\n", record.unique_id);
        	/* file name */
        	if(record.unique_id && (!ast_strlen_zero(record.unique_id))){
            		snprintf(cdr_full_path,sizeof(cdr_full_path),"%s/%s",cdr_dir_tmp,record.unique_id);
			/* write cdr to file if rc_acct failed */
			ret = save_avp_to_file(cdr_full_path,record.unique_id,send);
			if(!ret){
				ast_log(LOG_ERROR,"Failed to write cdr to file! unique_id=%s\n", record.unique_id);
			}
        	}
		goto return_cleanup;
	}

return_cleanup:
	if (send) {
		rc_avpair_free(send);
	}
}

static int unload_module(void)
{
	ast_cel_backend_unregister(RADIUS_BACKEND_NAME);
	if (rh) {
		rc_destroy(rh);
		rh = NULL;
	}
	return AST_MODULE_LOAD_SUCCESS;
}

static int load_module(void)
{
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };
	const char *tmp;

	if ((cfg = ast_config_load(cel_config, config_flags))) {
		ast_set2_flag(&global_flags, ast_true(ast_variable_retrieve(cfg, "radius", "usegmtime")), RADIUS_FLAG_USEGMTIME);
		if ((tmp = ast_variable_retrieve(cfg, "radius", "radiuscfg"))) {
			ast_copy_string(radiuscfg, tmp, sizeof(radiuscfg));
		}
		ast_config_destroy(cfg);
	} else {
		return AST_MODULE_LOAD_DECLINE;
	}

	/* create dir /var/lib/cdr if it does not exist. add by liucl */    
	if (access(cdr_directory,F_OK) == -1){
		ast_log(LOG_DEBUG,"cdr_directory %s is not exist, I will create it.\n",cdr_directory);
		if(mkdir(cdr_directory, 0755) == -1) {
			ast_log(LOG_ERROR,"Failed to create %s\n", cdr_directory);
		}else{
			ast_log(LOG_DEBUG,"Create directory %s is OK\n",cdr_directory);
		}
	}
	/* liucl add end*/

	/*
	 * start logging
	 *
	 * NOTE: Yes this causes a slight memory leak if the module is
	 * unloaded.  However, it is better than a crash if cdr_radius
	 * and cel_radius are both loaded.
	 */
	tmp = ast_strdup("asterisk");
	if (tmp) {
		rc_openlog((char *) tmp);
	}

	/* read radiusclient-ng config file */
	if (!(rh = rc_read_config(radiuscfg))) {
		ast_log(LOG_NOTICE, "Cannot load radiusclient-ng configuration file %s.\n", radiuscfg);
		return AST_MODULE_LOAD_DECLINE;
	}

	/* read radiusclient-ng dictionaries */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))) {
		ast_log(LOG_NOTICE, "Cannot load radiusclient-ng dictionary file.\n");
		rc_destroy(rh);
		rh = NULL;
		return AST_MODULE_LOAD_DECLINE;
	}

	if (ast_cel_backend_register(RADIUS_BACKEND_NAME, radius_log)) {
		rc_destroy(rh);
		rh = NULL;
		return AST_MODULE_LOAD_DECLINE;
	} else {
/*
 * 		* Create a independent thread to monitoring /var/lib/cdr. 
 * 				* If there is file in the directory, then send it to radius.
 * 						* add by liucl
 * 								*/
		start_monitor();
		return AST_MODULE_LOAD_SUCCESS;
	}
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "RADIUS CEL Backend",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CDR_DRIVER,
);
